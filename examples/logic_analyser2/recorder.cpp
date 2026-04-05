//
// recorder.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2016-2026  R. Stange <rsta2@gmx.net>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "recorder.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include <stdio.h>
#include <assert.h>

#define min(a, b)	((a) < (b) ? (a) : (b))

extern "C"
{
	bool logic_analyser_run (PIO pio, unsigned pin_base, unsigned m_nPinCount, float div,
				 u32 *capture_buf, size_t capture_size_words,
				 unsigned trigger_pin, bool trigger_level);

	void print_capture_buf (const u32 *buf, unsigned pin_base, unsigned m_nPinCount,
				u32 n_samples);

	unsigned get_buf_size_words (unsigned m_nPinCount, unsigned n_samples);

	unsigned bits_packed_per_word (unsigned pin_count);
}

CRecorder::CRecorder (unsigned nPinBase, unsigned nPinCount)
:	m_nPinBase (nPinBase),
	m_nPinCount (nPinCount),
	m_pBuffer (nullptr),
	m_nBufferSizeWords (0),
	m_nMemoryDepth (MAX_MEMORY_DEPTH),
	m_nTriggerPin (0),
	m_fDivider (1.0f),
	m_bRunning (FALSE)
{
}

CRecorder::~CRecorder (void)
{
	delete [] m_pBuffer;
	m_pBuffer = 0;
}

boolean CRecorder::Initialize (void)
{
	m_nBufferSizeWords = get_buf_size_words (m_nPinCount, m_nMemoryDepth);
	assert (m_nBufferSizeWords);

	assert (!m_pBuffer);
	m_pBuffer = new u32[m_nBufferSizeWords];

	return !!m_pBuffer;
}

unsigned CRecorder::GetPin (unsigned nChannel) const
{
	assert (nChannel > 0);
	assert (nChannel <= m_nPinCount);
	return m_nPinBase + nChannel-1;
}

void CRecorder::SetMemoryDepth (unsigned nMemoryDepth)
{
	assert (nMemoryDepth <= MAX_MEMORY_DEPTH);
	m_nMemoryDepth = nMemoryDepth;

	m_nBufferSizeWords = get_buf_size_words (m_nPinCount, m_nMemoryDepth);
}

void CRecorder::SetTrigger (boolean bEnabled, unsigned nChannel, boolean bLevel)
{
	if (bEnabled)
	{
		m_nTriggerPin = GetPin (nChannel);
		m_bTriggerLevel = bLevel;
	}
	else
	{
		m_nTriggerPin = 0;
	}
}

void CRecorder::SetClockDivider (float fDivider)
{
	assert (fDivider >= 1.0f);
	m_fDivider = fDivider;
}

boolean CRecorder::Run (void)
{
	if (m_bRunning)
	{
		return FALSE;
	}

	m_bRunning = TRUE;

	PIO pio = pio0;

	if (!logic_analyser_run (pio, m_nPinBase,  m_nPinCount, m_fDivider,
				 m_pBuffer, m_nBufferSizeWords,
				 m_nTriggerPin, m_bTriggerLevel))
	{
		m_bRunning = FALSE;

		return FALSE;
	}

	// print_capture_buf (m_pBuffer, m_nPinBase, m_nPinCount, min (m_nMemoryDepth, 64));

	m_bRunning = FALSE;

	return TRUE;
}

unsigned CRecorder::GetRuntime (void) const
{
	return static_cast<unsigned> (1000000.0f * m_nMemoryDepth / MAX_SAMPLE_RATE * m_fDivider);
}

unsigned CRecorder::GetSample (unsigned nChannel, unsigned nOffset) const
{
	unsigned record_size_bits = bits_packed_per_word (m_nPinCount);
	unsigned pin = nChannel-1;
	unsigned bit_index = pin + nOffset * m_nPinCount;
	unsigned word_index = bit_index / record_size_bits;
	// Data is left-justified in each FIFO entry, hence the (32 - record_size_bits) offset
	unsigned word_mask = 1u << (bit_index % record_size_bits + 32 - record_size_bits);

	return m_pBuffer[word_index] & word_mask ? HIGH : LOW;
}

const void *CRecorder::GetSampleBuffer (void) const
{
	assert (m_pBuffer);
	return m_pBuffer;
}

size_t CRecorder::GetSampleBufferSize (void) const
{
	return m_nBufferSizeWords * sizeof (u32);
}
