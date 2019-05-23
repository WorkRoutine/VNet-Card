#ifndef _BASE_H_
#define _BASE_H_

#include <queue.h>

#define BUFFER_SIZE	0x40000

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

#endif
