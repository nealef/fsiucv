#ifndef _FSIUCV_H
# define _FSIUCV_H

/************************************************************/
/*                                                          */
/*                     DEFINES                              */
/*                     -------                              */
/*                                                          */
/************************************************************/

#include <linux/ioctl.h>

/*
 * Ioctl definitions
 */

/* Use 'u' as magic number */
#define FSIUCV_IOC_MAGIC  'u'

#define FSIUCV_IOCRESET _IO(FSIUCV_IOC_MAGIC, 0)
#define FSIUCVTCS	_IOW(FSIUCV_IOC_MAGIC, 1, int)
#define FSIUCVTCG	_IOR(FSIUCV_IOC_MAGIC, 1, int)
#define FSIUCVSCS	_IOW(FSIUCV_IOC_MAGIC, 2, int)
#define FSIUCVSCG	_IOR(FSIUCV_IOC_MAGIC, 2, int)
#define FSIUCVMTS	_IOW(FSIUCV_IOC_MAGIC, 3, int)
#define FSIUCVMTG	_IOR(FSIUCV_IOC_MAGIC, 3, int)
#define FSIUCVANS	_IOW(FSIUCV_IOC_MAGIC, 4, size_t)
#define FSIUCVVMG	_IOR(FSIUCV_IOC_MAGIC, 5, char)
#define FSIUCVNDG	_IOR(FSIUCV_IOC_MAGIC, 6, char)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

/* ... more to come */

#define FSIUCV_IOCHARDRESET _IO(FSIUCV_IOC_MAGIC, 1)

/*=============== End of Defines ===========================*/

#endif
