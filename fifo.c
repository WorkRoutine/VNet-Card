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
    unsigned long *front; 
    unsigned long *rear;

    /* Clear all Reserved memory */
    memset((unsigned long *)base, 0, RESERVED_SIZES);
    fifo_magic = (unsigned int *)(base + MAGIC_OFFSET);
    *fifo_magic = FIFO_MAGIC;
    front = (unsigned int *)(base + HEAD_FRONT_OFFSET);
    rear = (unsigned int *)(base + HEAD_REAR_OFFSET);
    *front = NODE_NUM; /* NODE_NUM is NULL */
    *rear =  NODE_NUM; /* NODE_NUM is NULL */

    return 0;
}

void fifo_release(unsigned long base)
{
    memset((unsigned long *)base, 0, RESERVED_SIZES);
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
    unsigned long *front = (unsigned int *)(base + HEAD_FRONT_OFFSET);
    unsigned long *rear  = (unsigned int *)(base + HEAD_REAR_OFFSET);

    p = alloc_node(base);
    if (p == NULL)
        return -1;
    else {
        p->base = 0;
        p->size = 0;
        p->next = NODE_NUM; /* NODE_NUM is NULL */
        *front = p->index;
        *rear  = p->index;
        return 0;
    }
}

/*
 * Verify FIFO is empty
 */
int IsQueueEmpty(unsigned long base)
{
    unsigned long *front = (unsigned int *)(base + HEAD_FRONT_OFFSET);
    unsigned long *rear  = (unsigned int *)(base + HEAD_REAR_OFFSET);

    if (*front == *rear)
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
    struct node *node;
    struct node *node_list = (struct node *)(begin + NODE_OFFSET);
    unsigned long *front = (unsigned int *)(begin + HEAD_FRONT_OFFSET);
    unsigned long *rear  = (unsigned int *)(begin + HEAD_REAR_OFFSET);

    p = alloc_node(begin);
    if (p == NULL)
        return -1;

    p->base = base;
    p->size = size;
    p->next = NODE_NUM;
    
    node = node_list + *rear;
    node->next = p->index;
    *rear = p->index;

    return 0;
}

/*
 * Pop node from FIFO
 */
int PopElement(unsigned long base, unsigned long *pbase, unsigned long *psize)
{
    struct node *p;
    struct node *node;
    struct node *node_list = (struct node *)(base + NODE_OFFSET);
    unsigned long *front = (unsigned int *)(base + HEAD_FRONT_OFFSET);
    unsigned long *rear  = (unsigned int *)(base + HEAD_REAR_OFFSET);

    if (IsQueueEmpty((unsigned long)base))
        return -1;

    node = node_list + *front;
    p = node_list + node->next;
    *pbase = p->base;
    *psize = p->size;
    node->next = p->next;
    if (node->next == NODE_NUM)
        *rear = *front;

    free_node(base, p);

    return 0;
}

/* 
 * Obtain data from FIFO head node.
 */
int GetHeadElement(unsigned long base, unsigned long *pbase, 
                                              unsigned long *psize)
{
    struct node *node_list = (struct node *)(base + NODE_OFFSET);
    unsigned long *front = (unsigned int *)(base + HEAD_FRONT_OFFSET);
    unsigned long *rear  = (unsigned int *)(base + HEAD_REAR_OFFSET);
    struct node *node;

    if (IsQueueEmpty(base)) {
        *pbase = 0;
        *psize = 0;
        return 0;
    }

    node = node_list + *rear;

    *pbase = node->base;
    *psize = node->size;

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
