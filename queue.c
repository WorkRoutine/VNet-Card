/*
 * Queue.
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
int queue_write(struct queue_node *node, const char *buf, int count)
{
	int i;
	long valid_count = 0;

	if (!buf) {
		printf("Empty data to write\n");
		return -EINVAL;
	}

	/* valid byte to write */
	//valid_count = 8192 - queueW_cnt_get(node);

	/* Pls wait to test and next set */

	for (i = 0; i < count; i++)
		queue_data_write(node, buf[i]);

	return count;
}

int queue_read(struct queue_node *node, char *buf)
{
	int i;
	int count;

	if ((queueR_flag_get(node) & 0x1))
		return 0;

	if (!(count = queueR_cnt_get(node)))
		return 0;

	for (i = 0; i < count; i++)
		buf[i] = queue_data_read(node);

	return count;
}

/* initialize queue */
struct queue_node *queue_init(void)
{
	queue_mmap(&Def_queue);

	return &Def_queue;
}

void queue_exit(struct queue_node *node)
{
	queue_unmap(node);
}
