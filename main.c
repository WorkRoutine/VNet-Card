/*
 * Virtual net card.
 *
 * (C) 2019.05.14 <buddy.zhang@aliyun.com>
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
	int ret;

	while (1) {
		long count;
		int retry = 0;
	
		/* Read from Tun/Tap */
		count = read(vc->tun_fd, (char *)vc->WBuf, BUFFER_SIZE);
		if (count <= 0) {
			/* no data and sleep */
			usleep(MAX_DELAY);
			continue;
		}
		ret = queue_write(vc->queue, vc->WBuf, count);
		while (ret == -EAGAIN) {
			if (retry > 100) {
				/* give up send message */
				usleep(MAX_DELAY);
				retry = 0;
				/* clear write queue */
				queue_clear(vc->queue);
				continue;
			}
			retry++;
			usleep(MAX_DELAY); /* need optimization */
			ret = queue_write(vc->queue, vc->WBuf, count);
		}
	}
}

/* Rece procedure */
void *recv_procedure(void *arg)
{
	struct vc_node *vc = (struct vc_node *)arg;
	int ret;

	while (1) {
		long count;

		count = queue_read(vc->queue, vc->RBuf);

		if (count > 0) {
			unsigned long retry_num = count;
			unsigned long finish_num = 0;

			/* Receive data */
			ret = write(vc->tun_fd, (char *)vc->RBuf, count);
			while (retry_num) {
				if (ret < 0) {
					usleep(MAX_DELAY); /* retry tap/tun */
				} else {
					retry_num  -= ret;
					finish_num += ret;
				}
				if (!retry_num)
					break;
		
				ret = write(vc->tun_fd, (char *)vc->RBuf + 
						finish_num, retry_num);
			}
		} else {
			/* no data */
			usleep(MAX_DELAY);
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
