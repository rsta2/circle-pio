//
// rp1pio.h
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
#ifndef _rp1pio_h
#define _rp1pio_h

#include <circle/device.h>
#include <circle/interrupt.h>
#include <circle/dmachannel-rp1.h>
#include <circle/sched/semaphore.h>
#include <circle/sched/mutex.h>
#include <circle/spinlock.h>
#include <circle/types.h>
#include <stdint.h>
#include "rp1firmware.h"
#include "rp1_pio_if.h"

class CRP1PIO : public CDevice
{
public:
	static const unsigned RP1_PIO_SMS_COUNT   = 4;
	static const unsigned RP1_PIO_INSTR_COUNT = 32;

	static const unsigned DMA_BOUNCE_BUFFER_SIZE = 0x1000;
	static const unsigned DMA_BOUNCE_BUFFER_COUNT = 4;

public:
	CRP1PIO (CInterruptSystem *pInterrupt);
	~CRP1PIO (void);

	boolean Initialize (void);

	int IOCtl (unsigned long ulCmd, void *pData);

private:
	struct rp1_pio_client
	{
		volatile u32 claimed_sms;
		volatile u32 claimed_instrs;
		volatile u32 claimed_dmas;
	};

	struct dma_buf_info
	{
		void *buf;
		size_t len;
	};

	struct dma_info
	{
		CSemaphore *buf_sem;
		CDMAChannelRP1 *chan;
		uintptr fifo_addr;
		bool direction_to_dev;
		CDMAChannelRP1::TDREQ dreq;
		size_t buf_size;
		size_t buf_count;
		unsigned head_idx;
		unsigned tail_idx;
		dma_buf_info bufs[DMA_BOUNCE_BUFFER_COUNT];
	};

private:
	int rp1_pio_message (u16 op, const void *data, unsigned int data_len);
	int rp1_pio_message_resp (u16 op, const void *data, unsigned int data_len,
				  void *resp, void *userbuf, unsigned int resp_len);
	int rp1_pio_read_hw (rp1_pio_client *client, void *param);
	int rp1_pio_write_hw (rp1_pio_client *client, void *param);
	int rp1_pio_find_program (rp1_pio_add_program_args *prog);
	void rp1_pio_remove_instrs (u32 mask);
	static void rp1_pio_sm_dma_callback (unsigned nchan, unsigned nbuf,
					     bool status, void *param);
	void rp1_pio_sm_dma_free (dma_info *dma);
	int rp1_pio_sm_config_xfer_internal (rp1_pio_client *client, unsigned sm, unsigned dir,
					     unsigned buf_size, unsigned buf_count);
	int rp1_pio_sm_config_xfer_user (rp1_pio_client *client, void *param);
	int rp1_pio_sm_config_xfer32_user (rp1_pio_client *client, void *param);
	int rp1_pio_sm_tx_user (dma_info *dma, const void *userbuf, size_t bytes);
	int rp1_pio_sm_rx_user (dma_info *dma, void *userbuf, size_t bytes);
	int rp1_pio_sm_xfer_data32_user (rp1_pio_client *client, void *param);
	int rp1_pio_sm_xfer_data_user (rp1_pio_client *client, void *param);
	void rp1_pio_close (rp1_pio_client *client);
	int rp1_pio_handler (unsigned long ioctl_num, rp1_pio_client *client, void *param);
	int rp1_pio_ioctl  (unsigned long ioctl_num, void *ioctl_param);
	int rp1_pio_probe (void);

	int rp1_pio_can_add_program (rp1_pio_client *client, void *param);
	int rp1_pio_add_program (rp1_pio_client *client, void *param);
	int rp1_pio_remove_program (rp1_pio_client *client, void *param);
	int rp1_pio_clear_instr_mem (rp1_pio_client *client, void *param);
	int rp1_pio_sm_claim (rp1_pio_client *client, void *param);
	int rp1_pio_sm_unclaim (rp1_pio_client *client, void *param);
	int rp1_pio_sm_is_claimed (rp1_pio_client *client, void *param);
	int rp1_pio_sm_init (rp1_pio_client *client, void *param);
	int rp1_pio_sm_set_config (rp1_pio_client *client, void *param);
	int rp1_pio_sm_exec (rp1_pio_client *client, void *param);
	int rp1_pio_sm_clear_fifos (rp1_pio_client *client, void *param);
	int rp1_pio_sm_set_clkdiv (rp1_pio_client *client, void *param);
	int rp1_pio_sm_set_pins (rp1_pio_client *client, void *param);
	int rp1_pio_sm_set_pindirs (rp1_pio_client *client, void *param);
	int rp1_pio_sm_set_enabled (rp1_pio_client *client, void *param);
	int rp1_pio_sm_restart (rp1_pio_client *client, void *param);
	int rp1_pio_sm_clkdiv_restart (rp1_pio_client *client, void *param);
	int rp1_pio_sm_enable_sync (rp1_pio_client *client, void *param);
	int rp1_pio_sm_put (rp1_pio_client *client, void *param);
	int rp1_pio_sm_get (rp1_pio_client *client, void *param);
	int rp1_pio_sm_set_dmactrl (rp1_pio_client *client, void *param);
	int rp1_pio_sm_fifo_state (rp1_pio_client *client, void *param);
	int rp1_pio_sm_drain_tx (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_init (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_set_function (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_set_pulls (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_set_outover (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_set_inover (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_set_oeover (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_set_input_enabled (rp1_pio_client *client, void *param);
	int rp1_pio_gpio_set_drive_strength (rp1_pio_client *client, void *param);
	int rp1_pio_sm_config_xfer (rp1_pio_client *client, void *param);
	int rp1_pio_sm_xfer_data (rp1_pio_client *client, void *param);

private:
	CInterruptSystem *m_pInterrupt;
	CRP1Firmware m_Firmware;

	u16 m_fw_pio_base;
	u16 m_fw_pio_count;

	volatile u32 m_claimed_sms;

	u32 m_claimed_dmas;
	dma_info m_dma_configs[RP1_PIO_SMS_COUNT][RP1_PIO_DIR_COUNT];

	u32 m_used_instrs;
	u8 m_instr_refcounts[RP1_PIO_INSTR_COUNT];
	u16 m_instrs[RP1_PIO_INSTR_COUNT];

	rp1_pio_client m_client;	// only one client allowed

	CMutex m_instr_lock;
	CSpinLock m_spinlock;
};

#endif
