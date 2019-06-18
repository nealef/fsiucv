#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <iconv.h>
#include <syslog.h>
#include <string.h>
#include "fsiucv.h"

/* 
 * This sample assumes we're playing with the *MSG service but it should be
 * able to drive any connection
 */

typedef struct iucvMsg {
	char userid[8];
	char msgText[1016];
} iucvMsg;

static char *class[] = { "[MSG] ", "[WNG] ", "[CPIO]", "[SMSG]",
			 "[VMIO]", "[EMSG]", "[IMSG]", "[SCIF]"
		       };

int
main(int argc, char **argv)
{
	iconv_t cd;
	int fd, i_buf, trgcls;
	char buffer[1024], output[1024];
	char *pBuffer, *pOutput, userName[9];
	size_t	count,
		outCount,
		*pCount = (size_t *) & count, 
		*pOutCount = (size_t *) & outCount, 
		iconvSz;
	iucvMsg *msgData = (iucvMsg *) & output[0];

	printf("User ID  Class\tMessage\n"
	       "-------- -----\t--------------------------------\n");
	userName[8] = 0;
	cd = iconv_open("ASCII", "EBCDIC-US");
	fd = open("/dev/iucv0", O_RDONLY);
	if (fd >= 0) {
		count = read(fd, &buffer, sizeof (buffer));
		while (count > 0) {
			pBuffer = (char *) &buffer;
			pOutput = (char *) &output;
			ioctl(fd, FSIUCVTCG, (char *) &trgcls);
			trgcls--;
			outCount = sizeof (output);
			iconvSz = iconv(cd, &pBuffer, pCount, &pOutput, pOutCount);
			output[sizeof (output) - outCount] = 0;
			memcpy(userName, msgData->userid, sizeof (msgData->userid));
			printf("%s %s\t%s\n", 
			       userName, class[trgcls], msgData->msgText);
			syslog(LOG_INFO, "%s %s %s\n",
			       class[trgcls], userName, msgData->msgText);
			if (strcmp("STOP", msgData->msgText) == 0)
				break;
			count = read(fd, &buffer, sizeof (buffer));
		}
		close(fd);
	} else
		perror("open");
	iconv_close(cd);
}
