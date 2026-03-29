//
// rp1mailbox.h
//
// Ported to Circle by Rene Stange
//
// Based on the Linux driver, which is:
//	drivers/mailbox/rp1-mailbox.c
//	RP1 mailbox IPC driver
//	Author: Phil Elwell <phil@raspberrypi.com>
//	Copyright (C) 2023 Raspberry Pi Ltd.
//	SPDX-License-Identifier: GPL-2.0
//
// Parts of this driver are based on:
//  - bcm2835-mailbox.c
//    Copyright (C) 2010,2015 Broadcom
//    Copyright (C) 2013-2014 Lubomir Rintel
//    Copyright (C) 2013 Craig McGeachie
//
#ifndef _rp1mailbox_h
#define _rp1mailbox_h

#include <circle/interrupt.h>
#include <circle/spinlock.h>
#include <circle/types.h>

class CRP1Mailbox	// Acts as a doorbell
{
public:
	static const unsigned MaxChans = 4;	/* 32 is the hardware limit */

	typedef void TRxHandler (unsigned nChannel, void *pParam);

public:
	CRP1Mailbox (CInterruptSystem *pInterrupt);
	~CRP1Mailbox (void);

	void EnableChannel (unsigned nChannel);
	void DisableChannel (unsigned nChannel);

        void SendData (unsigned nChannel);
        boolean LastTxDone (unsigned nChannel);

	void RegisterRxHandler (unsigned nChannel, TRxHandler *pHandler, void *pParam);

private:
	unsigned chan_event (unsigned chan);
	static void mbox_irq (void *pParam);
	int send_data (unsigned chan);
	int startup (unsigned chan);
	void shutdown (unsigned chan);
	bool last_tx_done (unsigned chan);
	int mbox_probe (void);

private:
	CInterruptSystem *m_pInterrupt;

	TRxHandler *m_pRxHandler[MaxChans];
	void *m_pParam[MaxChans];

	CSpinLock m_SpinLock;
};

#endif
