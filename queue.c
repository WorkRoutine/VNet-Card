/*
 * Queue.
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

/* queue header file */
#include <queue.h>

#ifdef CONFIG_HOST
/* define Host queue_node */
static struct queue_node Def_queue = {
	.name = "X86_queue",
	.Rqueue_base = 0x10000,
	.Wqueue_base = 0x20000,
	.Rqueue_size = 0x2000,
	.Wqueue_size = 0x2000
};
#endif

#ifdef CONFIG_FPGA
/* define FPGA queue_node */
static struct queue_node Def_queue = {
	.name = "FPGA_queue",
	.Rqueue_base = 0xA0320000,
	.Wqueue_base = 0xA0300000,
	.Rqueue_size = 0x2000,
	.Wqueue_size = 0x2000,
};
#endif

/* queue map */
static int queue_mmap(struct queue_node *node)
{
	int fd;

#ifdef CONFIG_HOST
	fd = open("/dev/xdma0_user", O_RDWR | O_SYNC);
#elif defined CONFIG_FPGA
	fd = fd = open("/dev/mem", O_RDWR);
#endif

	/* Mmap Read_Queue */
	node->Rqueue = (unsigned long)mmap(0, node->Rqueue_size, PROT_READ | 
				PROT_WRITE, MAP_SHARED, fd, 
				(off_t)(node->Rqueue_base));
	if ((unsigned long *)node->Rqueue == NULL) {
		printf("Unable mmap %#lx\n", node->Rqueue_base);
		return -EINVAL;
	}

	/* Mmap Write_Queue */
	node->Wqueue = (unsigned long)mmap(0, node->Wqueue_size, PROT_READ |
				PROT_WRITE, MAP_SHARED, fd, 
				(off_t)(node->Wqueue_base));
	if ((unsigned long *)node->Wqueue == NULL) {
		printf("Unable mmap %#lx\n", node->Wqueue_base);
		munmap((void *)node->Rqueue, node->Rqueue_size);
		return -EINVAL;
	}

	close(fd);
	return 0;
}

/* queue umap */
static void queue_unmap(struct queue_node *node)
{
	munmap((void *)node->Wqueue, node->Wqueue_size);
	munmap((void *)node->Rqueue, node->Rqueue_size);
}

#include <sys/time.h>
#include <time.h>
static unsigned long Value = 0;

int queue_write_test(struct queue_node *node)
{
	uint32_t *pData = (uint32_t *)((unsigned long)node->Wqueue + QUE_DATA);
	unsigned long Count = 0;
	unsigned long Total = 0;

	while (1) {
		long cnt = 8192 - queueW_cnt_get(node);

		if (cnt < 4096) {
			usleep(200);
			continue;
		} else {
			int i;
			struct timeval start;
			struct timeval end;

			gettimeofday(&start,NULL);
			for (i = 0; i < cnt; i++) {
				*pData = Value++;
			}
			gettimeofday(&end,NULL);
			Total += end.tv_sec * 1000000 + end.tv_usec - 
				  (start.tv_sec * 1000000 + start.tv_usec);

			Count += cnt;
			if (Count > 4000000) {
				printf("Rate: %f Mbps\n",
						 (float)(Count * 4 * 8) / (float)Total);
				Count = 0;
				Total = 0;
			}
		}
	}
		
	return 0;
}


int queue_read_test(struct queue_node *node)
{
	uint32_t *pData = (uint32_t *)((unsigned long)node->Rqueue + QUE_DATA);
	unsigned long Total = 0;
	long Done_size = 0;

	while (1) {
		long cnt = queueR_cnt_get(node);

		if (cnt < 4096) {
			usleep(200);
			continue;
		} else {
			int i;
			unsigned int value;
			struct timeval start;
			struct timeval end;

			gettimeofday(&start,NULL);
			for (i = 0; i < cnt; i++)
				value = *pData;
			gettimeofday(&end,NULL);
			Total += end.tv_sec * 1000000 + end.tv_usec - 
				  (start.tv_sec * 1000000 + start.tv_usec);

			Done_size += cnt;
			if (Done_size > 4000000) {
				printf("Read %ld Time %f\n", Done_size,
							 (float)Total);
				printf("Rate: %f Mbps\n",
                                                 (float)(Done_size * 4 * 8) / (float)Total);
				Done_size = 0;
				Total = 0;
			}
		}
	}

	return 0;
}

/* queue read */
int queue_write(struct queue_node *node, const char *buf, int count)
{
	int i;
	long valid_count = 0;
	int done_size = count;
	int first_frame = 1;

	queue_data_write(node, 0xEF);

	if (!buf) {
		printf("Empty data to write\n");
		return -EINVAL;
	}

	/* valid byte to write */
	valid_count = QUEUE_SIZE - queueW_cnt_get(node);
	valid_count *= 2;
	if (count > valid_count)
		return -EAGAIN;

	/* Pls wait to test and next set */

	for (i = 0; i < count; i += 2) {
		unsigned int data = 0;

		if (done_size > 1) {
			data |= (buf[i] & 0xFF) | ((buf[i+1] & 0xFF) << 8);
			data |= DATA_FULL << 16;
			done_size -= 2;
		} else if (done_size = 1) {
			data |= (buf[i] & 0xFF) | (0x00 << 8);
			data |= DATA_HALF << 16;
			done_size -= 1;
		}

		/* Check endless */
		if (!done_size) {
			data |= FRAME_END << 24;
		}

		/* Frsit frame */
		if (first_frame) {
			data |= FRAME_FIRST << 24;
			first_frame = 0;
		}

		queue_data_write(node, data & 0xFFFFFFFF);
	}

	return count;
}

int queue_read(struct queue_node *node, char *buf)
{
	int i;
	int count;
	unsigned int flags = queueR_flag_get(node);
	int first = 1;
	int done_size = 0;

	printf("DAFDSFASD: flags: %#x\n", flags);
	if (flags & 0x1)
		return 0;

	printf("DDDDD\n");
	count = queueR_cnt_get(node);
	if (!count)
		return 0;

	for (i = 0; i < count; i++) {
		unsigned int data;
		unsigned char half;

		data = queue_data_read(node) & 0xFFFFFFFF;
		half = (data >> 16) & 0xFF;

		/* Head check */
		if (first) {
			if (((data >> 24) & 0xFF) == FRAME_FIRST)
				first = 0;
		}

		if (half == DATA_FULL) {
			buf[done_size++] = data & 0xFF;
			buf[done_size++] = (data >> 8) & 0xFF;
		} else if (half == DATA_HALF) {
			buf[done_size++] = data & 0xFF;
		}

		if (((data >> 24) & 0xFF) == FRAME_END)
			return done_size;
		
	}
	return count;
}

/* initialize queue */
struct queue_node *queue_init(void)
{
	queue_mmap(&Def_queue);

	queue_clear(&Def_queue);

	return &Def_queue;
}

void queue_exit(struct queue_node *node)
{
	queue_unmap(node);
}
