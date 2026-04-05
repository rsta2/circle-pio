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
#include "webserver.h"
#include <circle/string.h>
#include <circle/startup.h>

#define CLOCK_RATE0		50000
#define CLOCK_RATE1		100000
#define CLOCK_RATE2		500000

// Network configuration
#define NET_DEVICE_TYPE		NetDeviceTypeEthernet		// or: NetDeviceTypeWLAN

#define USE_DHCP

#ifndef USE_DHCP
static const u8 IPAddress[]      = {192, 168, 0, 250};
static const u8 NetMask[]        = {255, 255, 255, 0};
static const u8 DefaultGateway[] = {192, 168, 0, 1};
static const u8 DNSServer[]      = {192, 168, 0, 1};
#endif

CKernel::CKernel(void)
:	CStdlibAppNetwork ("logic_analyser", CSTDLIBAPP_DEFAULT_PARTITION,
#ifndef USE_DHCP
	IPAddress, NetMask, DefaultGateway, DNSServer,
#else
	0, 0, 0, 0,
#endif
	NET_DEVICE_TYPE),
	mPIO (CInterruptSystem::Get ()),
	mRecorder (21, 4),
	mClock0 (GPIOClock0),
	mClock1 (GPIOClock1),
	mClock2 (GPIOClock2),
	mClockPin0 (4, GPIOModeAlternateFunction0),
	mClockPin1 (5, GPIOModeAlternateFunction0),
	mClockPin2 (6, GPIOModeAlternateFunction0)
{
}

bool CKernel::Initialize (void)
{
	if (!CStdlibAppNetwork::Initialize ())
	{
		return false;
	}

	if (!mPIO.Initialize ())
	{
		return false;
	}

	return mRecorder.Initialize ();;
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
	if (!mClock0.StartRate (CLOCK_RATE0))
	{
		mLogger.Write (GetKernelName(), LogPanic,
			       "Cannot generate %u Hz clock", CLOCK_RATE0);
	}

	if (!mClock1.StartRate (CLOCK_RATE1))
	{
		mLogger.Write (GetKernelName(), LogPanic,
			       "Cannot generate %u Hz clock", CLOCK_RATE1);
	}

	if (!mClock2.StartRate (CLOCK_RATE2))
	{
		mLogger.Write (GetKernelName(), LogPanic,
			       "Cannot generate %u Hz clock", CLOCK_RATE2);
	}

	CString IPString;
	mNet.GetConfig ()->GetIPAddress ()->Format (&IPString);
	mLogger.Write (GetKernelName (), LogNotice, "Open \"http://%s/\" in your web browser!",
		       (const char *) IPString);

	new CWebServer (&mNet, &mRecorder);

	while (!is_power_button_pressed ())
	{
		mScheduler.MsSleep (100);
	}

	poweroff ();

	return ShutdownHalt;
}
