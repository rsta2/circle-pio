#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <circle/devicenameservice.h>
#include <circle/sched/scheduler.h>

namespace
{
	CDevice *s_pDevice = nullptr;
}

int access(const char *path, int amode)
{
	if (strcmp (path, "/dev/pio0") == 0)
	{
		return 0;
	}

	errno = ENOENT;

	return -1;
}

int ioctl(int fd, unsigned long request, ...)
{
	if (!s_pDevice)
	{
		s_pDevice = CDeviceNameService::Get ()->GetDevice ("pio0", FALSE);
	}

	if (s_pDevice)
	{
		va_list var;
		va_start (var, request);

		void *p = va_arg(var, void *);
		if (!p)
		{
			errno = EINVAL;

			return -1;
		}

		int ret = s_pDevice->IOCtl ((unsigned) request, p);

		va_end (var);

		if (ret < 0)
		{
			errno = EIO;

			return -1;
		}

		return ret;
	}

	errno = EIO;

	return -1;
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	CScheduler::Get()->usSleep(rqtp->tv_sec * 1000000U + rqtp->tv_nsec / 1000U);

	return 0;
}
