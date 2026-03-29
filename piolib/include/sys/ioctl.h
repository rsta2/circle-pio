#ifndef _SYS_IOCTL_H_
#define _SYS_IOCTL_H_

#ifdef __cplusplus
extern "C" {
#endif

#define IOCPARM_MASK    0x7f            /* parameters must be < 128 bytes */
#define IOC_VOID        0x20000000U     /* no parameters */
#define IOC_OUT         0x40000000U     /* copy out parameters */
#define IOC_IN          0x80000000U     /* copy in parameters */

#define _IO(x,y)        (IOC_VOID|(x<<8)|y)
#define _IOR(x,y,t)     (IOC_OUT|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y)
#define _IOW(x,y,t)     (IOC_IN|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y)
#define _IOWR(x,y,t)    (IOC_OUT|IOC_IN|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y)

#define _IOC_TYPE(nr)   (((nr) >> 8) & 0xff)
#define _IOC_NR(nr)     (((nr) >> 0) & 0xff)
#define _IOC_SIZE(nr)   (((nr) >> 16) & IOCPARM_MASK)

#define FIONBIO         _IOW('f', 126, int)     /* set/clear non-blocking i/o */
#define FIONREAD        _IOR('f', 127, long)    /* get # bytes to read */

int ioctl(int fd, unsigned long request, ...);

#ifdef __cplusplus
}
#endif

#endif
