#ifndef _BASE_H_
#define _BASE_H_

#include <queue.h>

#define BUFFER_SIZE	0x40000
#define MAX_DELAY	1000		/* usleep */
#define MAX_TRANS_TIMEOUT	10
#define MAX_DELAY_TAP	1000

#define RINGBUF_CHAIN_SIZE	0x100000
#define RINGBUF_CHAIN_NUM	8
#define RINGBUF_SIZE		(RINGBUF_CHAIN_SIZE * RINGBUF_CHAIN_NUM)
#define RINGBUF_MAGIC		0x91101688

#define H2C0_CH0	"/dev/xdma0_h2c_0"
#define C2H0_CH0	"/dev/xdma0_c2h_0"
#define WRITE_PHY_ADDR	0x3000000	
#define READ_PHY_ADDR	(WRITE_PHY_ADDR + RINGBUF_SIZE)

#define ALIGN_4B(x)	((x+3)& ~3)

typedef void * xHdl;
typedef struct
{
	int wFd;
	int rFd;
	uint32_t wPhyAddr;
	uint32_t rPhyAddr;
} xdma_t;

/* virtual card node */
struct vc_node {
	const char *name;
	struct queue_node *queue;

	/* Tun */
	char tun_name[32];
	int tun_fd;
	char *ip;

	/* Write/Read Buffer */
	char *WBuf;
	char *RBuf;

	/* DMA */
	xdma_t *Xdma;

	/* RingBuffer: Write Buffer */
	char *RingBuf;
	long ring_index;
	long ring_count;
	long pos; /* point to current valid data area */

	/* RingBuffer: Read Buffer */
	char *RingBuf2;
	long ring_index2;
	long ring_count2;
	long pos2; /* point to current valid data area */
};

struct msg_data {
	uint32_t magic;
	uint32_t count;
};

extern struct vc_node Defnode;

static inline struct vc_node *vc_root(void)
{
	return &Defnode;
}

extern struct vc_node *vc_init(void);
extern void vc_exit(struct vc_node *vc);

/* signal */
extern void signal_init(void);

/* TUN/TAP */
extern int tun_create(char *dev, int flags, char *ip);
extern void tun_release(int fd);

/* DMA */
extern void dma_test(struct vc_node *vc);
extern void *xdma_open(void);
extern void xdma_close(xHdl hdl);
extern uint8_t *align_alloc(uint32_t size);
extern int xdma_write(uint8_t *pDate, uint32_t dataLen, unsigned long offset,
                                              xHdl hdl);
extern int xdma_read(uint8_t *pDate, uint32_t dataLen, unsigned long offset,
                                                         xHdl hdl);
extern int dma_buffer_fill(struct vc_node *vc, const char *buf, int count);
extern int dma_buffer_split(struct vc_node *vc, char *buf, int *count);
extern int dma_buffer_send(struct vc_node *vc, unsigned long *index,
					unsigned long *count);
extern int dma_buffer_recv(struct vc_node *vc, unsigned long index,
					unsigned long count);
extern void debug_dump_socket_frame(const char *buf, unsigned long count, 
					const char *msg);

static int inline dma_buffer_is_full(struct vc_node *vc, int count)
{
	long safe_count = RINGBUF_CHAIN_SIZE - vc->pos -
						sizeof(struct msg_data);
	if (count > safe_count)
		return 1;
	else
		return 0;
}
#endif
