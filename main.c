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

	while (1) {
		int count;

		/* Read from Tun/Tap */
		count = read(vc->tun_fd, (char *)vc->WBuf, BUFFER_SIZE);

		/* Route 0: Timeout */
		if (count < 0) {
			/* No data to from Tun/Tap */
			usleep(MAX_DELAY_TAP);
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
					exit(1);
				}
			}
		} else { /* Route 1: Fill buffer */
#ifdef CONFIG_SOCKET_DEBUG
#ifdef CONFIG_HOST
			debug_dump_socket_frame(vc->WBuf, count, "Send");
#endif
#endif
			if (dma_buffer_is_full(vc, count)) {
				ret = data_send(vc);
				if (ret < 0) {
					exit(1);
				}
			}
			/* fill buffer */
			dma_buffer_fill(vc, vc->WBuf, count);
		}
	}

}

#/* Rece procedure */
void *recv_procedure(void *arg)
{
	struct vc_node *vc = (struct vc_node *)arg;
	int ret;

	while (1) {
		unsigned long index, count;
		int i, nbytes;

		ret = queue_recv_msg(vc->queue, &index, &count);
		if (ret != 0) {
			/* No data, sleep */
			usleep(MAX_DELAY);
			continue;
		} else {
retry:
			/* Recv DMA buffer */
			ret = dma_buffer_recv(vc, index, count);
			if (ret < 0) {
				printf("ERROR: can't recv DMA.\n");
				usleep(100);
				goto retry;
			}
			/* split tun/tap frame */
			for (i = 0; i < count; i++) {
				ret = dma_buffer_split(vc, (char *)vc->RBuf,
								&nbytes);
				if (ret < 0) {
					printf("NO DATA..\n\n\n");
					continue;
				}
#ifdef CONFIG_SOCKET_DEBUG
#ifdef CONFIG_FPGA
				debug_dump_socket_frame(vc->RBuf, nbytes, 
								   "Recv");
#endif
#endif
				ret = write(vc->tun_fd, (char *)vc->RBuf, 
								nbytes);
				if (ret < 0) {
					printf("Tun/Tap suspend...\n");
					usleep(1000);
					continue;
				}
			}
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
