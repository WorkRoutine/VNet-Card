/*
 * VnetCard: DMA
 *
 * (C) 2019.05.15 <buddy.zhang@aliyun.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>

/* header file */
#include <base.h>

/* DMA buffer fill */
int dma_buffer_fill(struct vc_node *vc, const char *buf, int count)
{
	struct msg_data msg = {
		.magic = RINGBUF_MAGIC,
		.count = count,
	};
	unsigned long pos;
	uint32_t *magic;

	/* Relocate next valid data area */
	pos = vc->pos + vc->ring_index * RINGBUF_CHAIN_SIZE;
	pos = pos + (unsigned long)vc->RingBuf;
	magic = (uint32_t *)pos;
	
	/* Fill Message Magic */
	magic[0] = msg.magic;
	magic[1] = msg.count;
	
	/* Relocate to data area */
	pos = pos + sizeof(struct msg_data);

	/* Copy data into buffer */
	memcpy((char *)pos, buf, count);

	/* update new position, need aligned 4 Byte */
	vc->pos = ALIGN_4B(vc->pos + sizeof(struct msg_data) + count);
	vc->ring_count++;
	return count;
}

/* DMA buffer split */
int dma_buffer_split(struct vc_node *vc, char *buf, int *count)
{
	struct msg_data msg = {
		.magic = RINGBUF_MAGIC,
	};
	unsigned long pos;
	uint32_t *magic;
	int i;
	
	/* No data to read */
	if (vc->ring_count2 <= 0) {
		return -ENOMEM;
	}

	/* Relocate to next frame */
	pos = vc->pos2 + vc->ring_index2 * RINGBUF_CHAIN_SIZE;
	pos = pos + (unsigned long)vc->RingBuf2;
	magic = (uint32_t *)pos;

	for (i = 0; i < (RINGBUF_CHAIN_SIZE - vc->pos2); i++) {
		if (magic[0] == msg.magic)
			break;
		magic++;
	}

	if (i == (RINGBUF_CHAIN_SIZE - vc->pos2)) {
		printf("Can't find valid data.\n");
		return -EINVAL;
	}

	/* Fill Message Magic */
	msg.magic = magic[0];
	msg.count = magic[1];

	/* Relocate to data area */
	pos = pos + sizeof(struct msg_data);

	/* Copy data into buffer */
	memcpy(buf, (char *)pos, msg.count);

	/* update new position, need aligned 4 Byte */
	vc->pos2 = ALIGN_4B(vc->pos2 + sizeof(struct msg_data) + msg.count);
	vc->ring_count2--;
	*count = msg.count;
	return 0;
}

/* DMA transfer from X86 to FPGA */
int dma_buffer_send(struct vc_node *vc, unsigned long *index, 
						unsigned long *count)
{
	unsigned long offset = vc->ring_index * RINGBUF_CHAIN_SIZE;
	uint8_t *pos = (uint8_t *)((unsigned long)vc->RingBuf + offset);
	int ret, retry = 3;

#ifdef CONFIG_HOST
	ret = xdma_write(pos, RINGBUF_CHAIN_SIZE, offset, vc->Xdma);
	if (ret < 0) {
		return ret;
	}

	/* Retry */
	while (ret != RINGBUF_CHAIN_SIZE && retry) {
		/* Need retry */
		usleep(MAX_DELAY);
		ret = xdma_write(pos, RINGBUF_CHAIN_SIZE, offset, vc->Xdma);
		if (ret < 0 || !retry) {
			return ret;
		}
		retry--;
	}
#endif /* X86 HOST */

	/* Update index and count */
	*index = vc->ring_index;
	*count = vc->ring_count;

	vc->ring_count = 0;
	vc->pos = 0;
	vc->ring_index++;
	/* Ring Overflower */ 
	if (vc->ring_index == RINGBUF_CHAIN_NUM)
		vc->ring_index = 0;

	return 0;
}

/* DMA transfer from FPGA to X86 */
int dma_buffer_recv(struct vc_node *vc, unsigned long index, 
						unsigned long count)
{
#ifdef CONFIG_HOST
	unsigned long offset = RINGBUF_CHAIN_SIZE * index;
	uint8_t *pos = (uint8_t *)((unsigned long)vc->RingBuf2 + offset);
	long ret, retry = 3;

	if (index > RINGBUF_CHAIN_NUM || index < 0) {
		/* Invalid index from queue */
		return -786;
	}

	/* Recevice Data from FPGA */
	ret = xdma_read(pos, RINGBUF_CHAIN_SIZE, offset, vc->Xdma);
	if (ret < 0) {
		usleep(MAX_DELAY);
		return ret;
	}

	/* Retry */
	while (ret != RINGBUF_CHAIN_SIZE && retry) {
		/* Need retry */
		usleep(MAX_DELAY);
		ret = xdma_read(pos, RINGBUF_CHAIN_SIZE, offset, vc->Xdma);
		if (ret < 0 || !retry) {
			return ret;
		}
		retry--;
	}
#endif /* X86 HOST */

	/* Update ringbuf */
	vc->ring_index2 = index;
	vc->ring_count2 = count;
	vc->pos2 = 0;

	return RINGBUF_CHAIN_SIZE;
}

void *xdma_open(void)
{
	xdma_t *pXdma = (xdma_t *)malloc(sizeof(xdma_t));

	if(NULL == pXdma)
		return NULL;
	memset(pXdma, 0, sizeof(xdma_t));

	pXdma->wFd = open(H2C0_CH0, O_RDWR);
	pXdma->rFd = open(C2H0_CH0, O_RDWR | O_NONBLOCK);
	if((pXdma->wFd<0) || (pXdma->rFd<0))
		goto ERROR;

	pXdma->wPhyAddr = WRITE_PHY_ADDR;
	pXdma->rPhyAddr = READ_PHY_ADDR;

	return (xHdl)pXdma;

ERROR:
	if(pXdma) {
		if(pXdma->wFd>0) {
			close(pXdma->wFd);
			pXdma->wFd = -1;
		}
		if(pXdma->rFd>0) {
			close(pXdma->rFd);
			pXdma->rFd = -1;
		}

		free(pXdma);
		pXdma = NULL;
	}
	return NULL;
}

static void pointer_reset(int fd, xHdl hdl, unsigned long offset)
{
	xdma_t *pXdma = (xdma_t *)hdl;
	uint32_t addr = (fd == pXdma->rFd) ? pXdma->rPhyAddr : pXdma->wPhyAddr;

	lseek(fd, addr + offset, SEEK_SET);
}

int xdma_write(uint8_t *pDate, uint32_t dataLen, unsigned long offset,
                                              xHdl hdl)
{
	int ret;
	xdma_t *pXdma = (xdma_t *)hdl;

	if(NULL == hdl)
		return -EINVAL;

	pointer_reset(pXdma->wFd, pXdma, offset);

	return write(pXdma->wFd, pDate, dataLen);
}

int xdma_read(uint8_t *pDate, uint32_t dataLen, unsigned long offset,
                                                         xHdl hdl)
{
	xdma_t *pXdma = (xdma_t *)hdl;

	if(NULL == hdl)
		return -EINVAL;

	pointer_reset(pXdma->rFd, pXdma, offset);

	return read(pXdma->rFd, pDate, dataLen);
}

void xdma_close(xHdl hdl)
{
	xdma_t *pXdma = (xdma_t *)hdl;
	if(NULL == hdl)
		return;

	close(pXdma->wFd);
	close(pXdma->rFd);
	free(hdl);
}

uint8_t *align_alloc(uint32_t size)
{
	void *p = NULL;

	posix_memalign((void **)&p, 4096/*alignment*/, size + 4096);
	assert(p);
	return (uint8_t *)p;
}

void dma_diagnose(struct vc_node *vc)
{
#ifdef CONFIG_HOST
	char *RBuf = (unsigned char *)align_alloc(4096);
	char *WBuf = (unsigned char *)align_alloc(4096);

	strcpy(WBuf, "Hello BiscuitOS");
	while (vc->flags) {
		int ret;
		/* Write to DMA */
		ret = xdma_write(WBuf, 4096, 0, vc->Xdma);
		if (ret < 0)
			xdma_write(WBuf, 4096, 0, vc->Xdma);
		sleep(1);
		memset(RBuf, 0, 100);
		xdma_read(RBuf, 12, 0, vc->Xdma);
		printf("Buf: %s\n", RBuf);
	}
#endif
}
