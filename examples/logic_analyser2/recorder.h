//
// recorder.h
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
#ifndef _recorder_h
#define _recorder_h

#include <circle/types.h>

#define MAX_MEMORY_DEPTH	500000

#define MIN_SAMPLE_RATE		1000000U
#define MAX_SAMPLE_RATE		200000000U

class CRecorder
{
public:
	CRecorder (unsigned nPinBase, unsigned nPinCount);
	~CRecorder (void);

	boolean Initialize (void);

	unsigned GetPin (unsigned nChannel) const;	// returns GPIO pin number for channel

	// must be called before Run()
	void SetMemoryDepth (unsigned nMemoryDepth);	// number of samples
	void SetTrigger (boolean bEnable, unsigned nChannel = 1, boolean bLevel = FALSE);
	void SetClockDivider (float fDivider);		// must be >= 1.0

	boolean Run (void);
	unsigned GetRuntime (void) const;		// microseconds, 0 if did not run before

	unsigned GetSample (unsigned nChannel, unsigned nOffset) const;
#define LOW	0
#define HIGH	1

	const void *GetSampleBuffer (void) const;
	size_t GetSampleBufferSize (void) const;

private:
	unsigned m_nPinBase;
	unsigned m_nPinCount;

	u32 *m_pBuffer;
	unsigned m_nBufferSizeWords;

	unsigned m_nMemoryDepth;
	unsigned m_nTriggerPin;		// 0: trigger disabled
	boolean m_bTriggerLevel;
	float m_fDivider;

	volatile boolean m_bRunning;
};

#endif
