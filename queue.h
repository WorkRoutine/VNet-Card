#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdint.h>

#define QUE_FLAG	0x00
#define QUE_FLAGS_CLR	0x04
#define SCR_PAD		0x1C
#define QUE_DATA	0x40

#define QUE_WR_CNT	0x0C
#define QUE_WR_LOCK	0x10

#define QUE_RD_CNT	0x08
#define QUE_RD_LOCK	0x14

#define QUEUE_SIZE	0x2000
#define QUEUE_MASK	(QUEUE_SIZE - 1)

#define DATA_FULL	0xEF
#define DATA_HALF	0xDF
#define FRAME_FIRST	0xAF
#define FRAME_END	0xBF


struct queue_node {
	const char *name;
	unsigned long Rqueue_base; /* Read Queue */
	unsigned long Wqueue_base; /* Write Queue */
	unsigned long Rqueue_size;
	unsigned long Wqueue_size;
	unsigned long Rqueue; /* Read Queue virtual address */
	unsigned long Wqueue; /* Write Queue virtual address */
};

static inline uint32_t queue_reg_read(uint64_t reg)
{
	return *(uint32_t *)(reg);
}

static inline void queue_reg_write(uint64_t reg, uint32_t data)
{
	*(uint32_t *)(reg) = data;
}

static inline uint32_t queue_data_read(struct queue_node *node)
{
	return queue_reg_read(node->Rqueue + QUE_DATA);
}

static inline void queue_data_write(struct queue_node *node, uint32_t data)
{
	queue_reg_write(node->Wqueue + QUE_DATA, data);
}

static inline int queue_flag_read(unsigned long base)
{
	return queue_reg_read(base + QUE_FLAG);
}

static inline int queueW_cnt_get(struct queue_node *node)
{
	return queue_reg_read(node->Wqueue + QUE_WR_CNT) &QUEUE_MASK;
}

static inline int queue_cnt_read(unsigned long base)
{
	return queue_reg_read(base +  QUE_RD_CNT) & QUEUE_MASK;
}

static inline int queue_lock(unsigned long base)
{
	queue_reg_write(base + QUE_RD_LOCK, 1);
}

static inline int queue_unlock(unsigned long base)
{
	queue_reg_write(base + QUE_RD_LOCK, 0);
}

static inline int queue_read_lock(unsigned long base)
{
	return queue_reg_read(base + QUE_RD_LOCK);
}

#define queueR_flag_get(node)	queue_flag_read(node->Rqueue)
#define queueW_flag_get(node)	queue_flag_read(node->Wqueue)
#define queueR_cnt_get(node)	queue_cnt_read(node->Rqueue)
#define queueR_lock(node)	queue_lock(node->Rqueue)
#define queueW_lock(node)	queue_lock(node->Wqueue)
#define queueR_unlock(node)	queue_unlock(node->Rqueue)
#define queueW_unlock(node)	queue_unlock(node->Wqueue)
#define queueR_read_lock(node)	queue_read_lock(node->Rqueue)
#define queueW_read_lock(node)	queue_read_lock(node->Wqueue)

static inline void queue_clear(struct queue_node *node)
{
	queue_reg_write(node->Wqueue + QUE_FLAGS_CLR, 1);
}

extern struct queue_node *queue_init(void);
extern void queue_exit(struct queue_node *node);
extern int queue_write(struct queue_node *node, const char *buf, int count);
extern int queue_read(struct queue_node *node, char *buf);
extern int queue_write_test(struct queue_node *node);
extern int queue_read_test(struct queue_node *node);

#endif
