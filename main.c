/*
 * Virtual-NetCare.
 *
 * (C) 2019.05.14 <buddy.zhang@auperastor.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <malloc.h>
#include <pthread.h>

/* core header file */
#include <base.h>

/* Send procedure */
void *send_procedure(void *arg)
{
	struct vc_node *vc = (struct vc_node *)arg;	

	while (1) {
		unsigned long count;	
	
		printf("Send...\n");
		/* Read from Tun/Tap */
		count = read(vc->tun_fd, vc->WBuf, BUFFER_SIZE);

		queue_write(vc->queue, vc->WBuf, count);
	}
}

/* Rece procedure */
void *recv_procedure(void *arg)
{
	struct vc_node *vc = (struct vc_node *)arg;

	while (1) {
		unsigned long count;

		sleep (1);
		if ((count = queue_read(vc->queue, vc->RBuf)) > 0) {
			/* Receive data */
			printf("Recv...\n");
			write(vc->tun_fd, vc->RBuf, count);
		}
	}
}

/*
 * Main entry
 */
int main()
{
	struct vc_node *vc;
	pthread_t sendt, recvt;

	/* init... */
	vc = vc_init();
	if (!vc) {
		printf("Faild to initialize virtual card.\n");
		return -1;
	}

	/* Establish two thread to send and receive data. */
	if (pthread_create(&sendt, NULL, send_procedure, (void *)vc) != 0) {
		printf("ERROR: Unable to create send pthread.\n");
		return -1;
	}
    
	if (pthread_create(&recvt, NULL, recv_procedure, (void *)vc) != 0) {
		printf("ERROR: Unable to create recv pthread.\n");
		return -1;
	}
    
	if (pthread_join(sendt, NULL) != 0) {
		printf("ERROR: Unable to join send pthread.\n");
		return -1;
	}

	if (pthread_join(recvt, NULL) != 0) {
		printf("ERROR: Unable to join recv pthread.\n");
		return -1;
	}
	
	/* exit... */
	vc_exit(vc);

        return 0;
}
