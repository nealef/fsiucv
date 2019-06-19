/**
 * @file riclient.c
 * @summary Sample program for use with RISERVER 
 * @description This simple program is the client half of the
 * sample program that comes with REXXIUCV package that lives
 * on the z/VM packages download page (http://www.vm.ibm.com/download).
 * It simply sends the EBCDIC '5' to the server who then returns
 * 5 lines of the RISERVER source file.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "example.h"

int
main(int argc, char **argv) 
{
	int fd;
	int i;
    size_t j;
	char res[256];
	char out[256];
    int count = 5;
	char data[4];

	fd = open("/dev/iucv0",O_RDWR);
	if (fd < 0) {
		perror("open");
		exit(fd);
	}
    
    j = snprintf(data, sizeof(data), "%d", count);

    a2e(data,j);

    /*
     * Send the request to the server
     */ 
	if (write(fd, &data, j) <= 0) {
		perror("write");
		exit(2);
	}

    /*
     * We are expecting "data" lines back from the server
     */ 
	for (i = 0; i < count; i++) {
		j = read(fd, res, sizeof(res));
		if (j <= 0) {
			perror("read");
			exit(1);
		}

        e2a(res,j);
		res[j] = 0;
        printf("%s\n",res);
	}
	close(fd);
}
