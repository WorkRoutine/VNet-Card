/*
 * VnetCard
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

static int data_send(struct vc_node *vc)
{
	long index, count;
	int ret;

	/* Send DMA buffer */
	ret = dma_buffer_send(vc, &index, &count);
	if (ret < 0) {
		printf("ERROR: DMA failed.\n");
		return -EINVAL;
	}

	/* Notify FPGA by queue */
	ret = queue_send_msg(vc->queue, index, count);
	if (ret < 0) {
		printf("ERROR: Queue failed.\n");
		return -EINVAL;
	}
	return 0;
}

/* Send procedure */
void *send_procedure(void *arg)
{
	struct vc_node *vc = (struct vc_node *)arg;
	long timeout = 0;
	int ret;

	while (vc->flags) {
		int count;

		/* Read from Tun/Tap */
		count = read(vc->tun_fd, (char *)vc->WBuf, BUFFER_SIZE);

		/* Route 0: Timeout */
		if (count < 0) {
			/* No data to from Tun/Tap */
			usleep(MAX_TIMEOUT_TAP);
			timeout++;
			if (timeout > MAX_TRANS_TIMEOUT) {
				timeout = 0;

				/* No data on DMA Buffer */
				if (!vc->ring_count) {
					usleep(MAX_DELAY);
					continue;
				}

				/* Send DMA buffer */
				ret = data_send(vc);
				if (ret < 0) {
					printf("DMA ERROR on Timeout\n");
					vc->flags = 0;
					continue;
				}
			}
		} else { /* Route 1: Fill buffer */
			/* Transfer if frame number bigger then MAX_FRAME, 
			 * or Buffer is full */
			if (vc->ring_count > MAX_FRAME ||
					dma_buffer_is_full(vc, count)) {
				ret = data_send(vc);
				if (ret < 0) {
					printf("DMA SEND ERROR.\n");
					vc->flags = 0;
					continue;
				}
			}
			/* fill buffer */
			dma_buffer_fill(vc, vc->WBuf, count);
		}
	}
	return NULL;

}

/* Rece procedure */
void *recv_procedure(void *arg)
{
	struct vc_node *vc = (struct vc_node *)arg;
	int ret;
	int loss_count = 0;

	while (vc->flags) {
		unsigned long index, count;
		int i;
		unsigned long nbytes;

		ret = queue_recv_msg(vc->queue, &index, &count);
		if (ret != 0) {
			/* No data, sleep */
#ifdef CONFIG_HOST
			usleep(MAX_TIMEOUT_TAP);
#else
			usleep(MAX_TIMEOUT_TAP);
#endif
			continue;
		} else {
			int retry = RETRY_MAX;

retry_dma:
			/* Recv DMA buffer */
			ret = dma_buffer_recv(vc, index, count);
			if (ret < 0) {
				if (ret == -786) {
					printf("Invalid DMA.\n");
					usleep(100);
					continue;
				}
				printf("Can't recv DMA: RET[%d].\n", ret);
				usleep(100);
				goto retry_dma;
			}

			/* split tun/tap frame */
			for (i = 0; i < count; i++) {
				ret = dma_buffer_split(vc, (char *)vc->RBuf,
							(int *)&nbytes);
				if (ret < 0) {
					printf("NO DATA..\n\n\n");
					continue;
				}

retry_tun:
				ret = write(vc->tun_fd, (char *)vc->RBuf, 
								nbytes);
				if (ret < 0) {
					loss_count++;
					if (loss_count > 100) {
						loss_count = 0;
					}
					if (retry) {
						usleep(100);
						retry--;
						goto retry_tun;
					} else {
						/* loss package */
						continue;
					}

				}
				retry = RETRY_MAX;
			}
		}
	}
	return NULL;
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
	printf("Exit.....\n");

        return 0;
}
