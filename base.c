/*
 * virtual net card.
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
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/* queue header file */
#include <queue.h>
#include <base.h>

#ifdef CONFIG_HOST
struct vc_node Defnode = {
        .name = "X64 Virtual Card",
	.ip   = "10.10.10.1",
};
#elif defined CONFIG_FPGA
struct vc_node Defnode = {
        .name = "FPGA Virtual Card",
	.ip   = "10.10.10.5",
};
#endif


/* Virtual Card init-route */
struct vc_node *vc_init(void)
{
	struct vc_node *vc;
	int fd;

	vc = vc_root();

	/* Initialize queue */
	vc->queue = queue_init();

	/* Initialize tun */
	vc->tun_fd = tun_create(vc->tun_name, IFF_TUN | IFF_NO_PI, vc->ip);
	if (vc->tun_fd < 0) {
		printf("ERROR: Can't establish TUN\n");
		goto err_tun;
	}

	/* Tap/Tun Write Buffer */
	vc->WBuf = (char *)malloc(BUFFER_SIZE);
	if (!vc->WBuf) {
		printf("ERROR: No enough memory for WBuf\n");
		goto err_WBuf;
	}
	memset(vc->WBuf, 0, BUFFER_SIZE);

	/* Tap/Tun Read Buffer */
	vc->RBuf = (char *)malloc(BUFFER_SIZE);
	if (!vc->RBuf) {
		printf("ERROR: No enough memory for RBuf.\n");
		goto err_RBuf;
	}
	memset(vc->RBuf, 0, BUFFER_SIZE);

#ifdef CONFIG_HOST
	/* DMA */
	vc->Xdma = xdma_open();
	if (vc->Xdma == NULL) {
		printf("ERROR: Unable to open DMA Channel!\n");
		goto err_dma;
	}
#endif

	/* Write RingBuf */
#ifdef CONFIG_HOST
	vc->RingBuf = (unsigned char *)align_alloc(RINGBUF_SIZE);
#else /* FPGA */
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0)
		goto err_share_mem;
	vc->RingBuf = (unsigned char *)mmap(0, RINGBUF_SIZE, 
				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
						(off_t)READ_PHY_ADDR);
#endif
	if (!vc->RingBuf) {
		printf("ERROR: Unable to alloc RingBuf.\n");
		goto err_ringbuf;
	}
	vc->ring_index = 0;
	vc->ring_count = 0;
	vc->pos = 0;

	/* Read RingBuf */
#ifdef CONFIG_HOST
	vc->RingBuf2 = (unsigned char *)align_alloc(RINGBUF_SIZE);
#else /* FPGA */
	vc->RingBuf2 = (unsigned char *)mmap(0, RINGBUF_SIZE, 
				PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
						(off_t)WRITE_PHY_ADDR);
	close(fd); /* pre-relase mmap-handle */
#endif
	if (!vc->RingBuf2) {
		printf("ERROR: Unable to alloc RingBuf2.\n");
		goto err_ringbuf2;
	}
	vc->ring_index2 = 0;
	vc->ring_count2 = 0;
	vc->pos2 = 0;

	/* Signal */
	signal_init();

	return vc;

#ifdef CONFIG_HOST
err_ringbuf2:
	free(vc->RingBuf);
err_ringbuf:
	xdma_close(vc->Xdma);
err_dma:
	free(vc->RBuf);
#endif
#ifdef CONFIG_FPGA
err_ringbuf2:
	munmap((void *)vc->RingBuf, RINGBUF_SIZE);
err_ringbuf:
	close(fd);
err_share_mem:
	free(vc->RBuf);
#endif
err_RBuf:
	free(vc->WBuf);
err_WBuf:
	close(vc->tun_fd);
err_tun:
	queue_exit(vc->queue);
err:
	return NULL;
}

/* Virtual Card exit-route */
void vc_exit(struct vc_node *vc)
{
#ifdef CONFIG_HOST
	free(vc->RingBuf2);
	free(vc->RingBuf);
	/* Exit dma */
	xdma_close(vc->Xdma);
#else
	munmap((void *)vc->RingBuf2, RINGBUF_SIZE);
	munmap((void *)vc->RingBuf, RINGBUF_SIZE);
#endif
	/* Free buffer */
	free(vc->RBuf);
	free(vc->WBuf);

	/* Remove TUN */
	close(vc->tun_fd);

	/* Remove queue */
	queue_exit(vc->queue);
	printf("\nSafe exit virtual card!\n");
}

/* debug helper */
void debug_dump_socket_frame(const char *buf, unsigned long count, 
					const char *msg)
{
	int i, j = 0;

	printf("\n*************%s-%ld*************\n", msg, count);
	for (i = 0; i < count; i++, j++) {
		if (j > 15) {
			printf("\n");
			j = 0;
		}
		printf("%#hhx ", buf[i]);
	}
	printf("\n\n");
}
