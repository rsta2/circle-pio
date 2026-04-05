//
// kernel.h
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
#ifndef _kernel_h
#define _kernel_h

#include <circle_stdlib_app.h>
#include <rp1pio.h>
#include "recorder.h"
#include <circle/gpioclock.h>
#include <circle/gpiopin.h>

class CKernel : public CStdlibAppNetwork
{
public:
	CKernel (void);

	bool Initialize (void);

	TShutdownMode Run (void);

private:
	CRP1PIO mPIO;
	CRecorder mRecorder;

	CGPIOClock mClock0;
	CGPIOClock mClock1;
	CGPIOClock mClock2;

	CGPIOPin mClockPin0;
	CGPIOPin mClockPin1;
	CGPIOPin mClockPin2;
};

#endif
