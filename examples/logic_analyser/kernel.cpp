//
// kernel.cpp
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
#include "kernel.h"
#include <circle/interrupt.h>

#define CLOCK_RATE	25000000

extern "C" int main(int argc, const char **argv);

CKernel::CKernel(void)
:	CStdlibAppStdio ("logic_analyser"),
	mPIO (CInterruptSystem::Get ()),
	mClock0 (GPIOClock0),
	mClockPin (4, GPIOModeAlternateFunction0)
{
}

bool CKernel::Initialize (void)
{
	if (!CStdlibAppStdio::Initialize ())
	{
		return false;
	}

	return mPIO.Initialize ();
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
	if (!mClock0.StartRate (CLOCK_RATE))
	{
		mLogger.Write (GetKernelName(), LogPanic, "Cannot generate %u Hz clock", CLOCK_RATE);
	}

	static const char *ArgV[] = {"kernel", nullptr};

	main (1, ArgV);

	mTimer.MsDelay (500);

	return ShutdownHalt;
}
