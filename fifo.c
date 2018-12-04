#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "fifo.h"

/*
 * 
 * +--------+------+--------+--------------------------------------+
 * |        |      |        |                                      |
 * | EFLAGS | Head | BitMap |             Nodes Memory             |
 * |        |      |        |                                      |
 * +--------+------+--------+--------------------------------------+
 *
 */

/*
 * Obtain a free node index.
 */
static int bitmap_alloc(unsigned long base)
{
    unsigned char *bitmap_node = (unsigned char *)(base + BITMAP_OFFSET);
    int i, j;

    if (bitmap_node == NULL)
        return -1;

    for (i = 0; i < BITMAP_BYTES; i++) {
        for (j = 0; j < 8; j++) {
            if (!((bitmap_node[i] >> j) & 0x1)) {
                bitmap_node[i] |= (1 << j);
                return (i * 8 + j);
            }
        }
    }
    return -1;
}

/*
 * Release a unused node by index.
 */
static int bitmap_free(unsigned long base, int offset)
{
    unsigned char *bitmap_node = (unsigned char *)(base + BITMAP_OFFSET);
    int i, j;

    if (bitmap_node == NULL)
        return -1;

    if (offset >= (BITMAP_BYTES * 8))
        return -1;

    i = offset / 8;
    j = offset % 8;

    bitmap_node[i] &= ~(1 << j);

    return 0;
}

/*
 * Initaizlie fifo .....
 */
int fifo_init(unsigned long base)
{
    unsigned int *fifo_magic;
    struct head *fifo_head;

    /* Clear all Reserved memory */
    memset((unsigned long *)base, 0, RESERVED_SIZES);
    fifo_magic = (unsigned int *)(base + MAGIC_OFFSET);
    *fifo_magic = FIFO_MAGIC;
    fifo_head = (struct head *)(base + HEAD_OFFSET);
    fifo_head->front = NULL;
    fifo_head->rear = NULL;

    return 0;
}

int reload_fifo(unsigned long base)
{
    return 0;
}

/*
 * Allocate new node
 */
static struct node *alloc_node(unsigned long base)
{
    int index;
    struct node *node;
    struct node *node_list = (struct node *)(base + NODE_OFFSET);

    index = bitmap_alloc(base);
    if (index < 0)
        return NULL;
    node = node_list + index;

    memset(node, 0, sizeof(struct node));

    node->index = index;
    return node;
}

/* Free node */
static void free_node(unsigned long base, struct node *node)
{
    bitmap_free(base, node->index);
}

int InitLinkQueue(unsigned long base)
{
    struct node *p;
    struct head *fifo_head = (struct head *)(base + HEAD_OFFSET);

    p = alloc_node(base);
    if (p == NULL)
        return -1;
    else {
        p->base = MEM_OFFSET;
        p->size = 0;
        p->next = NULL;
        fifo_head->front = p;
        fifo_head->rear = p;
        return 0;
    }
}

/*
 * Verify FIFO is empty
 */
int IsQueueEmpty(unsigned long base)
{
    struct head *fifo_head = (struct head *)(base + HEAD_OFFSET);

    if (fifo_head->front == fifo_head->rear)
        return 1;
    else
        return 0;
}

/*
 * Push new node into FIFO
 */
int PushElement(unsigned long begin, unsigned long base, unsigned long size)
{
    struct node *p;
    struct head *fifo_head = (struct head *)(base + HEAD_OFFSET);

    p = alloc_node(begin);
    if (p == NULL)
        return -1;

    p->base = base;
    p->size = size;
    p->next = NULL;
    fifo_head->rear->next = p;
    fifo_head->rear = p;

    return 0;
}

/*
 * Pop node from FIFO
 */
int PopElement(unsigned long base, unsigned long *pbase, unsigned long *psize)
{
    struct node *p;
    struct head *fifo_head = (struct head *)(base + HEAD_OFFSET);

    if (IsQueueEmpty(base))
        return -1;

    p = fifo_head->front->next;
    *pbase = p->base;
    *psize = p->size;
    fifo_head->front->next = p->next;
    if (fifo_head->front->next == NULL)
        fifo_head->rear = fifo_head->front;

    free_node(base, p);

    return 0;
}

/* 
 * Obtain data from FIFO head node.
 */
int GetHeadElement(unsigned long base, unsigned long *pbase, 
                                              unsigned long *psize)
{
    struct head *fifo_head = (struct head *)(base + HEAD_OFFSET);

    if (IsQueueEmpty(base)) {
        *pbase = 0;
        *psize = 0;
        return 0;
    }

    *pbase = fifo_head->rear->base;
    *psize = fifo_head->rear->size;

    return 0;
}

#if 0
int main(void)
{
    char *p;
    int i;

    p = malloc(4096);

    printf("Initialize.....\n");
    fifo_init((unsigned long)p);

    /* Initialize FIFO queue */
    if(InitLinkQueue() < 0) {
        printf("Failed to init FIFO.\n");
        return -1;
    }

    for (i = 0; i < 20; i++) {
        unsigned long base, size;

        GetHeadElement(&base, &size);
        printf("=>Base: %d Size: %d\n", base, size);
        if (PushElement(base + size, 100) < 0) {
            printf("Unable push data into FIFO.\n");
            return -1;
        }
    }

    while (!IsQueueEmpty()) {
        unsigned long base, size;

        PopElement(&base, &size);
        printf("Base: %d Size: %d\n", base, size);
        GetHeadElement(&base, &size);
        printf(">>Base: %d Size: %d\n", base, size);
    }
    

    printf("Hello World\n");


    return 0;
}
#endif
