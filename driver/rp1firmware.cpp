//
// rp1firmware.cpp
//
// Ported to Circle by Rene Stange
//
// Based on the Linux driver, which is:
//	drivers/firmware/rp1-fw.c
//	RP1 firmware driver
//	Author: Phil Elwell <phil@raspberrypi.com>
//	Copyright (C) 2023-24 Raspberry Pi Ltd.
//	SPDX-License-Identifier: GPL-2.0
//
// Parts of this driver are based on:
//  - raspberrypi.c, by Eric Anholt <eric@anholt.net>
//    Copyright (C) 2015 Broadcom
//
#include "rp1firmware.h"
#include <circle/bcm2712.h>
#include <circle/memio.h>
#include <circle/sched/scheduler.h>
#include <circle/sysconfig.h>
#include <circle/logger.h>
#include <circle/timer.h>
#include <circle/debug.h>
#include <circle/util.h>
#include <errno.h>
#include <assert.h>

#define RP1_MAILBOX_FIRMWARE	0

#define min(a, b)		((a) < (b) ? (a) : (b))

#define writel(val, addr)	write32 (addr, val)
#define readl(addr)		read32 (addr)

enum rp1_firmware_ops {
	MBOX_SUCCESS		= 0x0000,
	GET_FIRMWARE_VERSION	= 0x0001, // na -> 160-bit version
	GET_FEATURE		= 0x0002, // FOURCC -> op base (0 == unsupported), op count

	COMMON_COUNT
};

struct rp1_get_feature_resp {
	u32 op_base;
	u32 op_count;
};

LOGMODULE ("rp1fw");

CRP1Firmware::CRP1Firmware (CInterruptSystem *pInterrupt)
:	m_Mailbox (pInterrupt),
	m_buf (reinterpret_cast<u32 *> (ARM_RP1_FW_MBOX_BASE)),
	m_buf_size (ARM_RP1_FW_MBOX_END - ARM_RP1_FW_MBOX_BASE + 1)
{
}

CRP1Firmware::~CRP1Firmware (void)
{
	m_Mailbox.DisableChannel (RP1_MAILBOX_FIRMWARE);
}

boolean CRP1Firmware::Initialize (void)
{
	return !rp1_firmware_probe ();
}

int CRP1Firmware::DoMessage (u16 op, const void *data, unsigned data_len,
			     void *resp, unsigned resp_space)
{
	return rp1_firmware_message (op, data, data_len, resp, resp_space);
}

int CRP1Firmware::GetFeature (u32 fourcc, u32 *op_base, u32 *op_count)
{
	return rp1_firmware_get_feature (fourcc, op_base, op_count);
}

void CRP1Firmware::response_callback (unsigned nChannel, void *pParam)
{
	CRP1Firmware *pThis = static_cast<CRP1Firmware *> (pParam);
	assert (pThis);

	assert (!pThis->m_bCompletion);
	pThis->m_bCompletion = TRUE;
}

/*
 * Sends a request to the RP1 firmware and synchronously waits for the reply.
 * Returns zero or a positive count of response bytes on success, negative on
 * error.
 */

int CRP1Firmware::rp1_firmware_message(u16 op, const void *data, unsigned int data_len,
				       void *resp, unsigned int resp_space)
{
	int ret = 0;
	u32 rc;

	if (data_len + 4 > m_buf_size)
		return -EINVAL;

	m_TransactionLock.Acquire ();

	memcpy (&m_buf[1], data, data_len);
	writel(((u32) op << 16) | data_len, reinterpret_cast<uintptr> (m_buf));

	m_bCompletion = FALSE;
        m_Mailbox.SendData (RP1_MAILBOX_FIRMWARE);
	unsigned nStartTicks = CTimer::GetClockTicks();
	while (!m_bCompletion)
	{
		if (CTimer::GetClockTicks() - nStartTicks > CLOCKHZ)
		{
			ret = -ETIMEDOUT;
			break;
		}
#ifdef NO_BUSY_WAIT
		CScheduler::Get()->Yield();
#endif
	}

	if (ret == 0) {
		rc = readl(reinterpret_cast<uintptr> (m_buf));
		if (rc & 0x80000000) {
			ret = (s32)rc;
		} else {
			ret = min(rc, resp_space);
			memcpy (resp, &m_buf[1], ret);
		}
	}

	// DebugHexDump (m_buf, m_buf_size, From);

	m_TransactionLock.Release ();

	return ret;
}

int CRP1Firmware::rp1_firmware_get_feature(u32 fourcc, u32 *op_base, u32 *op_count)
{
	rp1_get_feature_resp resp;
	int ret;

	memset(&resp, 0, sizeof(resp));
	ret = rp1_firmware_message(GET_FEATURE,
				   &fourcc, sizeof(fourcc),
				   &resp, sizeof(resp));
	*op_base = resp.op_base;
	*op_count = resp.op_count;
	if (ret < 0)
		return ret;
	if (ret < (int) sizeof(resp) || !resp.op_base)
		return -EOPNOTSUPP;
	return 0;
}

int CRP1Firmware::rp1_firmware_probe(void)
{
	u32 version[5];
	int ret;

	m_Mailbox.RegisterRxHandler (RP1_MAILBOX_FIRMWARE, response_callback, this);
	m_Mailbox.EnableChannel (RP1_MAILBOX_FIRMWARE);

	ret = rp1_firmware_message(GET_FIRMWARE_VERSION, nullptr, 0, &version, sizeof(version));
	if (ret == sizeof(version)) {
		LOGNOTE ("RP1 Firmware version %08x%08x%08x%08x%08x",
			 version[0], version[1], version[2], version[3], version[4]);
	} else {
		LOGWARN ("Request failed (%d)", ret);

		return ret;
	}

	return 0;
}
