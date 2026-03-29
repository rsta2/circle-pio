//
// rp1firmware.h
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
#ifndef _rp1firmware_h
#define _rp1firmware_h

#include <circle/interrupt.h>
#include <circle/genericlock.h>
#include "rp1mailbox.h"
#include <circle/types.h>

class CRP1Firmware	// RP1 firmware access
{
public:
	CRP1Firmware (CInterruptSystem *pInterrupt);
	~CRP1Firmware (void);

	boolean Initialize (void);

	int DoMessage (u16 op, const void *data, unsigned data_len,
		       void *resp, unsigned resp_space);

	int GetFeature (u32 fourcc, u32 *op_base, u32 *op_count);

private:
	static void response_callback (unsigned nChannel, void *pParam);
	int rp1_firmware_message (u16 op, const void *data, unsigned int data_len,
				  void *resp, unsigned int resp_space);
	int rp1_firmware_get_feature (u32 fourcc, u32 *op_base, u32 *op_count);
	int rp1_firmware_probe (void);

private:
	CRP1Mailbox m_Mailbox;

	u32 *m_buf;		/* The shared buffer */
	u32 m_buf_size;		/* The size of the shared buffer */

	volatile boolean m_bCompletion;

	CGenericLock m_TransactionLock;
};

#endif
