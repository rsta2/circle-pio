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
#include <circle/sched/scheduler.h>
#include <circle/gpioclock.h>
#include <circle/gpiopin.h>
#include <rp1pio.h>

class CKernel : public CStdlibAppStdio
{
public:
	CKernel (void);

	bool Initialize (void);

	TShutdownMode Run (void);

private:
	CScheduler mScheduler;

	CRP1PIO mPIO;

	CGPIOClock mClock0;
	CGPIOPin mClockPin;
};

#endif
