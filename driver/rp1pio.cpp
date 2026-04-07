//
// rp1pio.cpp
//
// Ported to Circle by Rene Stange
//
// Based on the Linux driver, which is:
//	drivers/misc/rp1-pio.c
//	PIO driver for RP1
//	Author: Phil Elwell
//	Copyright (C) 2023-24 Raspberry Pi Ltd.
//	SPDX-License-Identifier: GPL-2.0
//
// Parts of this driver are based on:
// - vcio.c, by Noralf Trønnes
//   Copyright (C) 2010 Broadcom
//   Copyright (C) 2015 Noralf Trønnes
//   Copyright (C) 2021 Raspberry Pi (Trading) Ltd.
// - bcm2835_smi.c & bcm2835_smi_dev.c by Luke Wren
//   Copyright (c) 2015 Raspberry Pi (Trading) Ltd.
//
#include "rp1pio.h"
#include <circle/devicenameservice.h>
#include <circle/logger.h>
#include <circle/bcm2712.h>
#include <circle/sched/scheduler.h>
#include <circle/util.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <assert.h>
#include "rp1-fw-pio.h"

#define MAX_ARG_SIZE		256

#define RP1_PIO_FIFO_TX0	0x00
#define RP1_PIO_FIFO_TX1	0x04
#define RP1_PIO_FIFO_TX2	0x08
#define RP1_PIO_FIFO_TX3	0x0c
#define RP1_PIO_FIFO_RX0	0x10
#define RP1_PIO_FIFO_RX1	0x14
#define RP1_PIO_FIFO_RX2	0x18
#define RP1_PIO_FIFO_RX3	0x1c

#define RP1_PIO_DMACTRL_DEFAULT	0x80000104

#define ROUND_UP(x, y)	(((x) + (y) - 1) - (((x) + (y) - 1) % (y)))

#define min(a, b)	((a) < (b) ? (a) : (b))

LOGMODULE ("rp1pio");

static const char DeviceName[] = "pio";

CRP1PIO::CRP1PIO (CInterruptSystem *pInterrupt)
:	m_pInterrupt (pInterrupt),
	m_Firmware (pInterrupt),
	m_fw_pio_base (0),
	m_fw_pio_count (0),
	m_claimed_sms (0),
	m_claimed_dmas (0),
	m_used_instrs (0)
{
	memset (m_dma_configs, 0, sizeof m_dma_configs);

	memset (m_instr_refcounts, 0, sizeof m_instr_refcounts);
	memset (m_instrs, 0, sizeof m_instrs);

	memset (&m_client, 0, sizeof m_client);
}

CRP1PIO::~CRP1PIO (void)
{
	CDeviceNameService::Get ()->RemoveDevice (DeviceName, 0, FALSE);

	rp1_pio_close (&m_client);
}

boolean CRP1PIO::Initialize (void)
{
	return !rp1_pio_probe ();
}

int CRP1PIO::IOCtl (unsigned long ulCmd, void *pData)
{
	return rp1_pio_ioctl (ulCmd, pData);
}

int CRP1PIO::rp1_pio_message(u16 op, const void *data, unsigned int data_len)
{
	u32 rc;
	int ret;

	if (op >= m_fw_pio_count)
		return -EOPNOTSUPP;
	ret = m_Firmware.DoMessage(m_fw_pio_base + op,
				   data, data_len,
				   &rc, sizeof(rc));
	if (ret == 4)
		ret = rc;
	return ret;
}

int CRP1PIO::rp1_pio_message_resp(u16 op, const void *data, unsigned int data_len,
				  void *resp, void *userbuf, unsigned int resp_len)
{
	u32 resp_buf[1 + 32];
	int ret;

	if (op >= m_fw_pio_count)
		return -EOPNOTSUPP;
	if (resp_len + 4 >= sizeof(resp_buf))
		return -EINVAL;
	if (!resp && !userbuf)
		return -EINVAL;
	ret = m_Firmware.DoMessage(m_fw_pio_base + op,
				   data, data_len,
				   resp_buf, resp_len + 4);
	if (ret >= 4 && !resp_buf[0]) {
		ret -= 4;
		if (resp)
			memcpy(resp, &resp_buf[1], ret);
		else
			memcpy(userbuf, &resp_buf[1], ret);
	} else if (ret >= 0) {
		ret = -EIO;
	}
	return ret;
}

int CRP1PIO::rp1_pio_read_hw(rp1_pio_client *client, void *param)
{
	rp1_access_hw_args *args = (rp1_access_hw_args *) param;

	return rp1_pio_message_resp(READ_HW,
				    args, 8, NULL, args->data, args->len);
}

int CRP1PIO::rp1_pio_write_hw(rp1_pio_client *client, void *param)
{
	rp1_access_hw_args *args = (rp1_access_hw_args *) param;
	u32 write_buf[32 + 1];
	int len;

	len = min(args->len, sizeof(write_buf) - 4);
	write_buf[0] = args->addr;
	memcpy(&write_buf[1], args->data, len);
	return m_Firmware.DoMessage(m_fw_pio_base + WRITE_HW,
				    write_buf, 4 + len, NULL, 0);
}

int CRP1PIO::rp1_pio_find_program(rp1_pio_add_program_args *prog)
{
	unsigned start, end, prog_size;
	u32 used_mask;
	unsigned i;

	start = (prog->origin != RP1_PIO_ORIGIN_ANY) ? prog->origin : 0;
	end = (prog->origin != RP1_PIO_ORIGIN_ANY) ? prog->origin :
			(RP1_PIO_INSTRUCTION_COUNT - prog->num_instrs);
	prog_size = sizeof(prog->instrs[0]) * prog->num_instrs;
	used_mask = (u32)(~0) >> (32 - prog->num_instrs);

	/* Find the best match */
	for (i = start; i <= end; i++) {
		u32 mask = used_mask << i;

		if ((m_used_instrs & mask) != mask)
			continue;
		if (!memcmp(m_instrs + i, prog->instrs, prog_size))
			return i;
	}

	return -1;
}

int CRP1PIO::rp1_pio_can_add_program(rp1_pio_client *client, void *param)
{
	rp1_pio_add_program_args *args = (rp1_pio_add_program_args *) param;
	int offset;

	if (args->num_instrs > RP1_PIO_INSTR_COUNT ||
		((args->origin != RP1_PIO_ORIGIN_ANY) &&
		 (args->origin >= RP1_PIO_INSTR_COUNT ||
		  ((args->origin + args->num_instrs) > RP1_PIO_INSTR_COUNT))))
		return -EINVAL;

	m_instr_lock.Acquire();
	offset = rp1_pio_find_program(args);
	m_instr_lock.Release();
	if (offset >= 0)
		return offset;

	/* Don't send the instructions, just the header */
	return rp1_pio_message(PIO_CAN_ADD_PROGRAM, args,
			       offsetof(rp1_pio_add_program_args, instrs));
}

int CRP1PIO::rp1_pio_add_program(rp1_pio_client *client, void *param)
{
	rp1_pio_add_program_args *args = (rp1_pio_add_program_args *) param;
	int offset;
	unsigned i;

	if (args->num_instrs > RP1_PIO_INSTR_COUNT ||
		((args->origin != RP1_PIO_ORIGIN_ANY) &&
		 (args->origin >= RP1_PIO_INSTR_COUNT ||
		  ((args->origin + args->num_instrs) > RP1_PIO_INSTR_COUNT))))
		return -EINVAL;

	m_instr_lock.Acquire();
	offset = rp1_pio_find_program(args);
	if (offset < 0)
		offset = rp1_pio_message(PIO_ADD_PROGRAM, args, sizeof(*args));

	if (offset >= 0) {
		u32 used_mask;
		unsigned prog_size;

		used_mask = ((u32)(~0) >> (-args->num_instrs & 0x1f)) << offset;
		prog_size = sizeof(args->instrs[0]) * args->num_instrs;

		if ((m_used_instrs & used_mask) != used_mask) {
			m_used_instrs |= used_mask;
			memcpy(m_instrs + offset, args->instrs, prog_size);
		}
		client->claimed_instrs |= used_mask;
		for (i = 0; i < args->num_instrs; i++)
			m_instr_refcounts[offset + i]++;
	}
	m_instr_lock.Release();
	return offset;
}

void CRP1PIO::rp1_pio_remove_instrs(u32 mask)
{
	rp1_pio_remove_program_args args;
	unsigned i;

	m_instr_lock.Acquire();
	args.num_instrs = 0;
	for (i = 0; ; i++, mask >>= 1) {
		if ((mask & 1) && m_instr_refcounts[i] && !--m_instr_refcounts[i]) {
			m_used_instrs &= ~(1 << i);
			args.num_instrs++;
		} else if (args.num_instrs) {
			args.origin = i - args.num_instrs;
			rp1_pio_message(PIO_REMOVE_PROGRAM, &args, sizeof(args));
			args.num_instrs = 0;
		}
		if (!mask)
			break;
	}
	m_instr_lock.Release();
}

int CRP1PIO::rp1_pio_remove_program(rp1_pio_client *client, void *param)
{
	rp1_pio_remove_program_args *args = (rp1_pio_remove_program_args *) param;
	u32 used_mask;
	int ret = -ENOENT;

	if (args->num_instrs > RP1_PIO_INSTR_COUNT ||
		args->origin >= RP1_PIO_INSTR_COUNT ||
		(args->origin + args->num_instrs) > RP1_PIO_INSTR_COUNT)
		return -EINVAL;

	used_mask = ((u32)(~0) >> (32 - args->num_instrs)) << args->origin;
	if ((client->claimed_instrs & used_mask) == used_mask) {
		client->claimed_instrs &= ~used_mask;
		rp1_pio_remove_instrs(used_mask);
		ret = 0;
	}
	return ret;
}

int CRP1PIO::rp1_pio_clear_instr_mem(rp1_pio_client *client, void *param)
{
	m_instr_lock.Acquire();
	(void)rp1_pio_message(PIO_CLEAR_INSTR_MEM, NULL, 0);
	memset(m_instr_refcounts, 0, sizeof(m_instr_refcounts));
	m_used_instrs = 0;
	client->claimed_instrs = 0;
	m_instr_lock.Release();
	return 0;
}

int CRP1PIO::rp1_pio_sm_claim(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_claim_args *args = (rp1_pio_sm_claim_args *) param;
	int ret;

	m_instr_lock.Acquire();
	ret = rp1_pio_message(PIO_SM_CLAIM, args, sizeof(*args));
	if (ret >= 0) {
		if (args->mask)
			client->claimed_sms |= args->mask;
		else
			client->claimed_sms |= (1 << ret);
		m_claimed_sms |= client->claimed_sms;
	}
	m_instr_lock.Release();
	return ret;
}

int CRP1PIO::rp1_pio_sm_unclaim(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_claim_args *args = (rp1_pio_sm_claim_args *) param;

	m_instr_lock.Acquire();
	(void)rp1_pio_message(PIO_SM_UNCLAIM, args, sizeof(*args));
	client->claimed_sms &= ~args->mask;
	m_claimed_sms &= ~args->mask;
	m_instr_lock.Release();
	return 0;
}

int CRP1PIO::rp1_pio_sm_is_claimed(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_claim_args *args = (rp1_pio_sm_claim_args *) param;

	return rp1_pio_message(PIO_SM_IS_CLAIMED, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_init(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_init_args *args = (rp1_pio_sm_init_args *) param;

	return rp1_pio_message(PIO_SM_INIT, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_set_config(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_set_config_args *args = (rp1_pio_sm_set_config_args *) param;

	return rp1_pio_message(PIO_SM_SET_CONFIG, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_exec(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_exec_args *args = (rp1_pio_sm_exec_args *) param;

	return rp1_pio_message(PIO_SM_EXEC, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_clear_fifos(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_clear_fifos_args *args = (rp1_pio_sm_clear_fifos_args *) param;

	return rp1_pio_message(PIO_SM_CLEAR_FIFOS, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_set_clkdiv(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_set_clkdiv_args *args = (rp1_pio_sm_set_clkdiv_args *) param;

	return rp1_pio_message(PIO_SM_SET_CLKDIV, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_set_pins(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_set_pins_args *args = (rp1_pio_sm_set_pins_args *) param;

	return rp1_pio_message(PIO_SM_SET_PINS, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_set_pindirs(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_set_pindirs_args *args = (rp1_pio_sm_set_pindirs_args *) param;

	return rp1_pio_message(PIO_SM_SET_PINDIRS, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_set_enabled(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_set_enabled_args *args = (rp1_pio_sm_set_enabled_args *) param;

	return rp1_pio_message(PIO_SM_SET_ENABLED, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_restart(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_restart_args *args = (rp1_pio_sm_restart_args *) param;

	return rp1_pio_message(PIO_SM_RESTART, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_clkdiv_restart(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_restart_args *args = (rp1_pio_sm_restart_args *) param;

	return rp1_pio_message(PIO_SM_CLKDIV_RESTART, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_enable_sync(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_enable_sync_args *args = (rp1_pio_sm_enable_sync_args *) param;

	return rp1_pio_message(PIO_SM_ENABLE_SYNC, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_put(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_put_args *args = (rp1_pio_sm_put_args *) param;

	return rp1_pio_message(PIO_SM_PUT, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_get(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_get_args *args = (rp1_pio_sm_get_args *) param;
	int ret;

	ret = rp1_pio_message_resp(PIO_SM_GET, args, sizeof(*args),
				   &args->data, NULL, sizeof(args->data));
	if (ret >= 0)
		return offsetof(rp1_pio_sm_get_args, data) + ret;
	return ret;
}

int CRP1PIO::rp1_pio_sm_set_dmactrl(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_set_dmactrl_args *args = (rp1_pio_sm_set_dmactrl_args *) param;

	return rp1_pio_message(PIO_SM_SET_DMACTRL, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_sm_fifo_state(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_fifo_state_args *args = (rp1_pio_sm_fifo_state_args *) param;
	const int level_offset = offsetof(rp1_pio_sm_fifo_state_args, level);
	int ret;

	ret = rp1_pio_message_resp(PIO_SM_FIFO_STATE, args, sizeof(*args),
				   &args->level, NULL, sizeof(*args) - level_offset);
	if (ret >= 0)
		return level_offset + ret;
	return ret;
}

int CRP1PIO::rp1_pio_sm_drain_tx(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_clear_fifos_args *args = (rp1_pio_sm_clear_fifos_args *) param;

	return rp1_pio_message(PIO_SM_DRAIN_TX, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_init(rp1_pio_client *client, void *param)
{
	rp1_gpio_init_args *args = (rp1_gpio_init_args *) param;

	return rp1_pio_message(GPIO_INIT, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_set_function(rp1_pio_client *client, void *param)
{
	rp1_gpio_set_function_args *args = (rp1_gpio_set_function_args *) param;

	return rp1_pio_message(GPIO_SET_FUNCTION, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_set_pulls(rp1_pio_client *client, void *param)
{
	rp1_gpio_set_pulls_args *args = (rp1_gpio_set_pulls_args *) param;

	return rp1_pio_message(GPIO_SET_PULLS, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_set_outover(rp1_pio_client *client, void *param)
{
	rp1_gpio_set_args *args = (rp1_gpio_set_args *) param;

	return rp1_pio_message(GPIO_SET_OUTOVER, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_set_inover(rp1_pio_client *client, void *param)
{
	rp1_gpio_set_args *args = (rp1_gpio_set_args *) param;

	return rp1_pio_message(GPIO_SET_INOVER, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_set_oeover(rp1_pio_client *client, void *param)
{
	rp1_gpio_set_args *args = (rp1_gpio_set_args *) param;

	return rp1_pio_message(GPIO_SET_OEOVER, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_set_input_enabled(rp1_pio_client *client, void *param)
{
	rp1_gpio_set_args *args = (rp1_gpio_set_args *) param;

	return rp1_pio_message(GPIO_SET_INPUT_ENABLED, args, sizeof(*args));
}

int CRP1PIO::rp1_pio_gpio_set_drive_strength(rp1_pio_client *client, void *param)
{
	rp1_gpio_set_args *args = (rp1_gpio_set_args *) param;

	return rp1_pio_message(GPIO_SET_DRIVE_STRENGTH, args, sizeof(*args));
}

void CRP1PIO::rp1_pio_sm_dma_callback(unsigned nchan, unsigned nbuf,
				      bool status, void *param)
{
	assert (nbuf == 0);
	assert (status);

	dma_info *dma = (dma_info *) param;
	assert (dma);

	assert (dma->buf_sem);
	dma->buf_sem->Up();
}

void CRP1PIO::rp1_pio_sm_dma_free(struct dma_info *dma)
{
	assert (dma);
	if (dma->chan) {
		dma->chan->Cancel();
	}
	while (dma->buf_count > 0) {
		dma->buf_count--;
		delete [] (u8 *) dma->bufs[dma->buf_count].buf;
		dma->bufs[dma->buf_count].buf = nullptr;
	}

	delete dma->chan;
	dma->chan = nullptr;
}

int CRP1PIO::rp1_pio_sm_config_xfer_internal(rp1_pio_client *client, unsigned sm, unsigned dir,
					     unsigned buf_size, unsigned buf_count)
{
	rp1_pio_sm_set_dmactrl_args set_dmactrl_args;
	dma_info *dma;
	u32 dma_mask;
	int ret = 0;

	if (sm >= RP1_PIO_SMS_COUNT || dir >= RP1_PIO_DIR_COUNT)
		return -EINVAL;
	if ((buf_count || buf_size) &&
	    (!buf_size || (buf_size & 3) ||
	     !buf_count || buf_count > DMA_BOUNCE_BUFFER_COUNT))
		return -EINVAL;

	dma_mask = 1 << (sm * 2 + dir);

	dma = &m_dma_configs[sm][dir];

	m_spinlock.Acquire();
	if (m_claimed_dmas & dma_mask)
		rp1_pio_sm_dma_free(dma);
	m_claimed_dmas |= dma_mask;
	client->claimed_dmas |= dma_mask;
	m_spinlock.Release();

	memset (dma, 0, sizeof *dma);

	dma->buf_size = buf_size;
	/* Round up the allocations */
	buf_size = ROUND_UP(buf_size, PAGE_SIZE);
	dma->buf_sem = new CSemaphore (0);

	/* Allocate and configure a DMA channel */
	/* Careful - each SM FIFO has its own DREQ value */
	static const CDMAChannelRP1::TDREQ s_DREQ[][2] =
	{
		{CDMAChannelRP1::DREQSourcePIO0TX, CDMAChannelRP1::DREQSourcePIO0RX},
		{CDMAChannelRP1::DREQSourcePIO1TX, CDMAChannelRP1::DREQSourcePIO1RX},
		{CDMAChannelRP1::DREQSourcePIO2TX, CDMAChannelRP1::DREQSourcePIO2RX},
		{CDMAChannelRP1::DREQSourcePIO3TX, CDMAChannelRP1::DREQSourcePIO3RX}
	};
	unsigned txrx = dir == RP1_PIO_DIR_TO_SM ? 0 : 1;
	assert (m_pInterrupt);
	assert (!dma->chan);
	dma->chan = new CDMAChannelRP1(DMA_CHANNEL_RP1_FAST, m_pInterrupt);
	assert (dma->chan);
	dma->dreq = s_DREQ[sm][txrx];

	/* Alloc and map bounce buffers */
	for (dma->buf_count = 0; dma->buf_count < buf_count; dma->buf_count++) {
		dma_buf_info *dbi = &dma->bufs[dma->buf_count];

		dbi->buf = new u8[buf_size];
		if (!dbi->buf) {
			ret = -ENOMEM;
			goto err_dma_free;
		}
		dbi->len = 0;
	}

	dma->fifo_addr = ARM_PIO0_BASE;
	dma->fifo_addr += sm * (RP1_PIO_FIFO_TX1 - RP1_PIO_FIFO_TX0);
	dma->fifo_addr += (dir == RP1_PIO_DIR_TO_SM) ? RP1_PIO_FIFO_TX0 : RP1_PIO_FIFO_RX0;

	dma->direction_to_dev = dir == RP1_PIO_DIR_TO_SM;

	set_dmactrl_args.sm = sm;
	set_dmactrl_args.is_tx = (dir == RP1_PIO_DIR_TO_SM);
	set_dmactrl_args.ctrl = RP1_PIO_DMACTRL_DEFAULT;
	if (dir == RP1_PIO_DIR_FROM_SM)
		set_dmactrl_args.ctrl = (RP1_PIO_DMACTRL_DEFAULT & ~0x1f) | 1;

	ret = rp1_pio_sm_set_dmactrl(client, &set_dmactrl_args);
	if (ret)
		goto err_dma_free;

	return 0;

err_dma_free:
	rp1_pio_sm_dma_free(dma);

	m_spinlock.Acquire();
	client->claimed_dmas &= ~dma_mask;
	m_claimed_dmas &= ~dma_mask;
	m_spinlock.Release();

	return ret;
}

int CRP1PIO::rp1_pio_sm_config_xfer_user(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_config_xfer_args *args = (rp1_pio_sm_config_xfer_args *) param;

	return rp1_pio_sm_config_xfer_internal(client, args->sm, args->dir,
					       args->buf_size, args->buf_count);
}

int CRP1PIO::rp1_pio_sm_config_xfer32_user(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_config_xfer32_args *args = (rp1_pio_sm_config_xfer32_args *) param;

	return rp1_pio_sm_config_xfer_internal(client, args->sm, args->dir,
					       args->buf_size, args->buf_count);
}


int CRP1PIO::rp1_pio_sm_tx_user(struct dma_info *dma, const void *userbuf, size_t bytes)
{
	int ret = 0;

	/* Clean the slate - we're running synchronously */
	dma->head_idx = 0;
	dma->tail_idx = 0;

	while (bytes > 0) {
		size_t copy_bytes = min(bytes, dma->buf_size);
		dma_buf_info *dbi;

		/*
		 * Grab the next free buffer, waiting if one is full.
		 * We can have only one outstanding buffer.
		 */
		// was: if (dma->head_idx - dma->tail_idx == dma->buf_count)
		if (dma->head_idx != dma->tail_idx)
		{
			if (dma->buf_sem->DownWithTimeout (1000000)) {
				LOGERR("DMA bounce timed out");
				break;
			}
			dma->tail_idx++;
		}

		dbi = &dma->bufs[dma->head_idx % dma->buf_count];
		dbi->len = copy_bytes;

		memcpy(dbi->buf, userbuf, copy_bytes);

		userbuf = (u8 *)userbuf + copy_bytes;

		dma->chan->SetupIOWrite (dma->fifo_addr, dbi->buf, dbi->len, dma->dreq);
		dma->chan->SetCompletionRoutine (rp1_pio_sm_dma_callback, dma);

		/* Submit the buffer - the callback will kick the semaphore */
		dma->chan->Start();

		ret = 0;

		dma->head_idx++;
		bytes -= copy_bytes;
	}

	/* Block for completion */
	while (dma->tail_idx != dma->head_idx) {
		if (dma->buf_sem->DownWithTimeout (1000000)) {
			LOGERR("DMA wait timed out");
			ret = -ETIMEDOUT;
			break;
		}
		dma->tail_idx++;
	}

	return ret;
}

int CRP1PIO::rp1_pio_sm_rx_user(dma_info *dma, void *userbuf, size_t bytes)
{
	int ret = 0;

	/* Clean the slate - we're running synchronously */
	dma->head_idx = 0;
	dma->tail_idx = 0;

	while (bytes || dma->tail_idx != dma->head_idx) {
		size_t copy_bytes = min(bytes, dma->buf_size);
		dma_buf_info *dbi;

		/*
		 * Wait for the next RX to complete if at least one buffer is
		 * outstanding or we're finishing up. Differently from Linux we
		 * can have only one outstanding buffer.
		 */
		// was: if (!bytes || dma->head_idx - dma->tail_idx == dma->buf_count)
		if (!bytes || dma->head_idx != dma->tail_idx)
		{
			if (dma->buf_sem->DownWithTimeout (1000000)) {
				LOGERR ("DMA wait timed out");
				ret = -ETIMEDOUT;
				break;
			}

			dbi = &dma->bufs[dma->tail_idx++ % dma->buf_count];
			assert (dbi->len);
			memcpy(userbuf, dbi->buf, dbi->len);
			userbuf = (u8 *)userbuf + dbi->len;

			if (!bytes)
				continue;
		}

		dbi = &dma->bufs[dma->head_idx % dma->buf_count];
		dbi->len = copy_bytes;
		dma->chan->SetupIORead (dbi->buf, dma->fifo_addr, dbi->len, dma->dreq);
		dma->chan->SetCompletionRoutine (rp1_pio_sm_dma_callback, dma);

		/* Submit the buffer - the callback will kick the semaphore */
		dma->head_idx++;

		dma->chan->Start();

		bytes -= copy_bytes;
	}

	return ret;
}

int CRP1PIO::rp1_pio_sm_xfer_data32_user(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_xfer_data32_args *args = (rp1_pio_sm_xfer_data32_args *) param;
	dma_info *dma;

	if (args->sm >= RP1_PIO_SMS_COUNT || args->dir >= RP1_PIO_DIR_COUNT ||
	    !args->data_bytes || !args->data)
		return -EINVAL;

	dma = &m_dma_configs[args->sm][args->dir];

	if (args->dir == RP1_PIO_DIR_TO_SM)
		return rp1_pio_sm_tx_user(dma, args->data, args->data_bytes);
	else
		return rp1_pio_sm_rx_user(dma, args->data, args->data_bytes);
}

int CRP1PIO::rp1_pio_sm_xfer_data_user(rp1_pio_client *client, void *param)
{
	rp1_pio_sm_xfer_data_args *args = (rp1_pio_sm_xfer_data_args *) param;
	rp1_pio_sm_xfer_data32_args args32;

	args32.sm = args->sm;
	args32.dir = args->dir;
	args32.data_bytes = args->data_bytes;
	args32.data = args->data;

	return rp1_pio_sm_xfer_data32_user(client, &args32);
}

void CRP1PIO::rp1_pio_close(rp1_pio_client *client)
{
	unsigned claimed_dmas = client->claimed_dmas;
	int i;

	/* Free any allocated resources */

	for (i = 0; claimed_dmas; i++) {
		unsigned mask = (1 << i);

		if (claimed_dmas & mask) {
			dma_info *dma = &m_dma_configs[i >> 1][i & 1];

			claimed_dmas &= ~mask;
			rp1_pio_sm_dma_free(dma);
		}
	}

	m_spinlock.Acquire();
	m_claimed_dmas &= ~client->claimed_dmas;
	m_spinlock.Release();

	if (client->claimed_sms) {
		rp1_pio_sm_set_enabled_args se_args = {
			.mask = client->claimed_sms, .enable = 0
		};
		rp1_pio_sm_claim_args uc_args = {
			.mask = client->claimed_sms
		};

		rp1_pio_sm_set_enabled(client, &se_args);
		rp1_pio_sm_unclaim(client, &uc_args);
	}

	if (client->claimed_instrs)
		rp1_pio_remove_instrs(client->claimed_instrs);

	/* Reinitialise the SM? */
}

#define HANDLER(_n, _f)		case _IOC_NR(PIO_IOC_ ## _n):			\
					if (sz != _IOC_SIZE(PIO_IOC_ ## _n))	\
						break;				\
					return rp1_pio_ ## _f (client, param)

int CRP1PIO::rp1_pio_handler(unsigned long ioctl_num, rp1_pio_client *client, void *param)
{
	int sz = _IOC_SIZE (ioctl_num);
	switch (_IOC_NR (ioctl_num))
	{
		HANDLER(SM_CONFIG_XFER, sm_config_xfer_user);
		HANDLER(SM_XFER_DATA, sm_xfer_data_user);
		HANDLER(SM_XFER_DATA32, sm_xfer_data32_user);
		HANDLER(SM_CONFIG_XFER32, sm_config_xfer32_user);

		HANDLER(CAN_ADD_PROGRAM, can_add_program);
		HANDLER(ADD_PROGRAM, add_program);
		HANDLER(REMOVE_PROGRAM, remove_program);
		HANDLER(CLEAR_INSTR_MEM, clear_instr_mem);

		HANDLER(SM_CLAIM, sm_claim);
		HANDLER(SM_UNCLAIM, sm_unclaim);
		HANDLER(SM_IS_CLAIMED, sm_is_claimed);

		HANDLER(SM_INIT, sm_init);
		HANDLER(SM_SET_CONFIG, sm_set_config);
		HANDLER(SM_EXEC, sm_exec);
		HANDLER(SM_CLEAR_FIFOS, sm_clear_fifos);
		HANDLER(SM_SET_CLKDIV, sm_set_clkdiv);
		HANDLER(SM_SET_PINS, sm_set_pins);
		HANDLER(SM_SET_PINDIRS, sm_set_pindirs);
		HANDLER(SM_SET_ENABLED, sm_set_enabled);
		HANDLER(SM_RESTART, sm_restart);
		HANDLER(SM_CLKDIV_RESTART, sm_clkdiv_restart);
		HANDLER(SM_ENABLE_SYNC, sm_enable_sync);
		HANDLER(SM_PUT, sm_put);
		HANDLER(SM_GET, sm_get);
		HANDLER(SM_SET_DMACTRL, sm_set_dmactrl);
		HANDLER(SM_FIFO_STATE, sm_fifo_state);
		HANDLER(SM_DRAIN_TX, sm_drain_tx);

		HANDLER(GPIO_INIT, gpio_init);
		HANDLER(GPIO_SET_FUNCTION, gpio_set_function);
		HANDLER(GPIO_SET_PULLS, gpio_set_pulls);
		HANDLER(GPIO_SET_OUTOVER, gpio_set_outover);
		HANDLER(GPIO_SET_INOVER, gpio_set_inover);
		HANDLER(GPIO_SET_OEOVER, gpio_set_oeover);
		HANDLER(GPIO_SET_INPUT_ENABLED, gpio_set_input_enabled);
		HANDLER(GPIO_SET_DRIVE_STRENGTH, gpio_set_drive_strength);

		HANDLER(READ_HW, read_hw);
		HANDLER(WRITE_HW, write_hw);

	default:
		LOGWARN ("Unknown ioctl: %lx", ioctl_num);

		return -EOPNOTSUPP;
	}

	LOGWARN ("Wrong %lx argsize (%d)", ioctl_num, sz);

	return -EINVAL;
};

int CRP1PIO::rp1_pio_ioctl (unsigned long ioctl_num, void *argp)
{
	u32 argbuf[MAX_ARG_SIZE/sizeof(u32)];
	int sz = _IOC_SIZE(ioctl_num);
	if (sz)
	{
		memcpy(argbuf, argp, sz);
	}

	int ret = rp1_pio_handler(ioctl_num, &m_client, argbuf);
	// LOGDBG ("%s: %lx -> %d", __func__, ioctl_num, ret);
	if (ret > 0)
	{
		memcpy(argp, argbuf, ret);
	}

	return ret;
}

int CRP1PIO::rp1_pio_probe(void)
{
	if (!m_Firmware.Initialize ())
	{
		LOGWARN ("Failed to contact RP1 firmware");

		return -EOPNOTSUPP;
	}

	u32 op_base, op_count;
	int ret = m_Firmware.GetFeature(FOURCC_PIO, &op_base, &op_count);
	if (ret < 0)
		return ret;

	m_fw_pio_base = op_base;
	m_fw_pio_count = op_count;

	CDeviceNameService::Get ()->AddDevice (DeviceName, 0, this, FALSE);

	return 0;
}
