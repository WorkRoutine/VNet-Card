#ifndef _FIFO_H_
#define _FIFO_H_

/* Size must 16 Bytes */
struct node
{
    unsigned long index;
    unsigned long base;
    unsigned long size;
    struct node *next;
};

struct head 
{
    struct node *front;
    struct node *rear;
};

#define BASE_PHY_ADDR            (0x50000000)
#define FIFO_BUFFER              (0x400000)
#define X86_TO_FPGA_OFFSET       (0x00000000)
#define FPGA_TO_X86_OFFSET       (X86_TO_FPGA_OFFSET + FIFO_BUFFER)

#define TOTAL_SIZES              ((FIFO_BUFFER) * 2)
#define NODE_NUM                 (256)
#define NODE_SIZES               ((NODE_NUM + 1) * sizeof(struct node))
#define EFLAGS_BYTES             (4)
#define MAGIC_BYTES              (4)
#define HEAD_BYTES               (sizeof(struct head))
#define BITMAP_BYTES             ((NODE_NUM + 7) / 8)
#define EFLAGS_OFFSET            (0)
#define MAGIC_OFFSET             (EFLAGS_OFFSET + EFLAGS_BYTES)
#define HEAD_OFFSET              (MAGIC_OFFSET + MAGIC_BYTES)
#define BITMAP_OFFSET            (HEAD_OFFSET + HEAD_BYTES)
#define NODE_OFFSET              (BITMAP_OFFSET + BITMAP_BYTES)
#define RESERVED_SIZES           (NODE_OFFSET + NODE_SIZES)
#define MEM_OFFSET               (RESERVED_SIZES)
#define MEM_SIZES                (TOTAL_SIZES - MEM_OFFSET)
#define FIFO_MAGIC               0x91929400

extern int fifo_init(unsigned long base);
extern int IsQueueEmpty(void);
extern int PushElement(unsigned long base, unsigned long size);
extern int PopElement(unsigned long *pbase, unsigned long *psize);
extern int GetHeadElement(unsigned long *pbase, unsigned long *psize);
extern int reload_fifo(unsigned long base);
extern int InitLinkQueue(void);

#endif
