//
// rp1mailbox.cpp
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
#include "rp1mailbox.h"
#include <circle/bcm2712.h>
#include <circle/memio.h>
#include <circle/rp1int.h>
#include <assert.h>

/*
 * RP1's PROC_EVENTS register can generate interrupts on the M3 cores (when
 * enabled). The 32-bit register is treated as 32 events, all of which share a
 * common interrupt. HOST_EVENTS is the same in the reverse direction.
 */
#define SYSCFG_PROC_EVENTS		0x00000008
#define SYSCFG_HOST_EVENTS		0x0000000c
#define SYSCFG_HOST_EVENT_IRQ_EN	0x00000010
#define SYSCFG_HOST_EVENT_IRQ		0x00000014

#define HW_SET_BITS			0x00002000
#define HW_CLR_BITS			0x00003000

#define writel(val, addr)		write32 (addr, val)
#define readl(addr)			read32 (addr)

CRP1Mailbox::CRP1Mailbox (CInterruptSystem *pInterrupt)
:	m_pInterrupt (pInterrupt)
{
	for (unsigned i = 0; i < MaxChans; i++)
	{
		m_pRxHandler[i] = nullptr;
	}

	mbox_probe ();
}

CRP1Mailbox::~CRP1Mailbox (void)
{
	assert (m_pInterrupt);
	m_pInterrupt->DisconnectIRQ (RP1_IRQ_SYSCFG);
}

void CRP1Mailbox::EnableChannel (unsigned nChannel)
{
	startup (nChannel);
}

void CRP1Mailbox::DisableChannel (unsigned nChannel)
{
	shutdown (nChannel);
}

void CRP1Mailbox::SendData (unsigned nChannel)
{
	send_data (nChannel);
}

boolean CRP1Mailbox::LastTxDone (unsigned nChannel)
{
	return last_tx_done (nChannel);
}

void CRP1Mailbox::RegisterRxHandler (unsigned nChannel, TRxHandler *pHandler, void *pParam)
{
	assert (nChannel < MaxChans);
	m_pRxHandler[nChannel] = pHandler;
	m_pParam[nChannel] = pParam;
}

unsigned CRP1Mailbox::chan_event (unsigned chan)
{
	assert (chan < MaxChans);
	return 1 << chan;
}

void CRP1Mailbox::mbox_irq(void *pParam)
{
	CRP1Mailbox *pThis = static_cast<CRP1Mailbox *> (pParam);
	assert (pThis);

	unsigned evs = readl(ARM_SYSCFG_BASE + SYSCFG_HOST_EVENT_IRQ);
	writel(evs, ARM_SYSCFG_BASE + SYSCFG_HOST_EVENTS + HW_CLR_BITS);

	// LOGDBG ("IRQ (0x%X)", evs);

	for (unsigned chan = 0; evs; chan++)
	{
		unsigned mask = 1 << chan;
		if (evs & mask)
		{
			pThis->m_SpinLock.Acquire ();

			if (pThis->m_pRxHandler[chan])
			{
				pThis->m_pRxHandler[chan](chan, pThis->m_pParam[chan]);
			}

			pThis->m_SpinLock.Release ();

			evs &= ~mask;
		}
	}
}

int CRP1Mailbox::send_data(unsigned chan)
{
	unsigned int event = chan_event(chan);

	writel(event, ARM_SYSCFG_BASE + SYSCFG_PROC_EVENTS + HW_SET_BITS);

	return 0;
}

int CRP1Mailbox::startup(unsigned chan)
{
	unsigned int event = chan_event(chan);

	writel(event, ARM_SYSCFG_BASE + SYSCFG_HOST_EVENT_IRQ_EN + HW_SET_BITS);

	return 0;
}

void CRP1Mailbox::shutdown(unsigned chan)
{
	unsigned int event = chan_event(chan);

	writel(event, ARM_SYSCFG_BASE + SYSCFG_HOST_EVENT_IRQ_EN + HW_CLR_BITS);
}

bool CRP1Mailbox::last_tx_done(unsigned chan)
{
	unsigned int event = chan_event(chan);
	unsigned int evs;

	evs = readl(ARM_SYSCFG_BASE + SYSCFG_HOST_EVENT_IRQ);

	return !(evs & event);
}

int CRP1Mailbox::mbox_probe(void)
{
	assert (m_pInterrupt);
	m_pInterrupt->ConnectIRQ (RP1_IRQ_SYSCFG, mbox_irq, this);

	return 0;
}
