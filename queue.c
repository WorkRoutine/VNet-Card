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

/* queue read */
int queue_write(struct queue_node *node, const unsigned char *buf, int count)
{
	long valid_count = 0;
	struct queue_head msg = {
		.msg_head = QUEUE_HEAD_MAGIC,
		.length = 0,
	};
	queue_t *pData;
	int full_count, ualign_count;
	queue_t *pos, vdata = 0;
	int i, j = 0;

	if (!buf) {
		printf("Empty data to write\n");
		return -EINVAL;
	}

	/* valid byte to write */
	valid_count = QUEUE_SIZE - queueW_cnt_get(node);
	if (valid_count - QUEUE_RESERVED < 0)
		return -EAGAIN;
	valid_count = valid_count * QUEUE_WIDTH - QUEUE_RESERVED;
	if (count > valid_count)
		return -EAGAIN;

	msg.length = count;
	pData = (queue_t *)((unsigned long)node->Wqueue + QUE_DATA);
	/* Send Head */
	*pData = msg.msg_head;
	*pData = msg.length;

	full_count = msg.length / QUEUE_WIDTH;
	ualign_count = msg.length % QUEUE_WIDTH;
	pos = (queue_t *)buf;

        for (i = 0; i < full_count; i++) {
                *(queue_t *)pData = *pos;
                pos++;
        }

        if (ualign_count == 0)
                return msg.length;

        for (i = 0; i < ualign_count; i++) {
		vdata |= (buf[full_count * QUEUE_WIDTH + i] & 0xFF) 
				<< (i * 8);
        }
	*pData = vdata;

        return msg.length;




	for (i = 0; i < msg.length; i += QUEUE_WIDTH) {
		queue_t value = 0;

		if ((msg.length - i) >= QUEUE_WIDTH) {
			/* Need memcpy */
			memcpy((char *)&value, &buf[i], QUEUE_WIDTH);
		} else {
			memcpy((char *)&value, &buf[i], msg.length - i);
		}
		/* Wait data into queue */
		*pData = value;
	}

	return count;
}

int queue_read(struct queue_node *node, char *buf)
{
	int count;
	unsigned int flags = queueR_flag_get(node);
	queue_t *pData;
	struct queue_head msg;
	int full_count, ualign_count;
	queue_t *pos, vdata;
	int i, j;

	/* Empty queue */
	if (flags & 0x1)
		return 0;

	count = queueR_cnt_get(node);
	if (!count)
		return 0;

	pData = (queue_t *)((unsigned long)node->Rqueue + QUE_DATA);
	/* Read head of msg */
	for (i = 0; i < count; i++) {
		msg.msg_head = *(queue_t *)pData;
		if (msg.msg_head == QUEUE_HEAD_MAGIC)
			break;
	}
	/* Can't find valid frame */
	if (i == count)
		return 0;

	msg.length = *(queue_t *)pData;

	full_count = msg.length / QUEUE_WIDTH;
	ualign_count = msg.length % QUEUE_WIDTH;
	pos = (queue_t *)buf;

	for (i = 0; i < full_count; i++) {
		*pos = *(queue_t *)pData;
		pos++;
	}

	if (ualign_count == 0)
		return msg.length;

	vdata = *(queue_t *)pData;
	for (i = 0; i < ualign_count; i++) {
		buf[full_count * QUEUE_WIDTH + i] =
				(vdata >> (i * 8) & 0xFF);
	}

	return msg.length;
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
