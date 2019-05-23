/*
 * virtual net card.
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
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/* queue header file */
#include <queue.h>
#include <base.h>

#ifdef CONFIG_HOST
struct vc_node Defnode = {
        .name = "X64 Virtual Card",
	.ip   = "10.10.10.1",
};
#elif defined CONFIG_FPGA
struct vc_node Defnode = {
        .name = "FPGA Virtual Card",
	.ip   = "10.10.10.5",
};
#endif


/* Virtual Card init-route */
struct vc_node *vc_init(void)
{
	struct vc_node *vc;

	vc = vc_root();

	/* Initialize queue */
	vc->queue = queue_init();

	/* Initialize tun */
	vc->tun_fd = tun_create(vc->tun_name, IFF_TUN | IFF_NO_PI, vc->ip);
	if (vc->tun_fd < 0) {
		printf("ERROR: Can't establish TUN\n");
		goto err_tun;
	}

	/* Buffer */
	vc->WBuf = (char *)malloc(BUFFER_SIZE);
	if (!vc->WBuf) {
		printf("ERROR: No enough memory for WBuf\n");
		goto err_WBuf;
	}
	memset(vc->WBuf, 0, BUFFER_SIZE);

	vc->RBuf = (char *)malloc(BUFFER_SIZE);
	if (!vc->RBuf) {
		printf("ERROR: No enough memory for RBuf.\n");
		goto err_RBuf;
	}
	memset(vc->RBuf, 0, BUFFER_SIZE);

	/* Signal */
	signal_init();

	return vc;

err_RBuf:
	free(vc->WBuf);
err_WBuf:
	close(vc->tun_fd);
err_tun:
	queue_exit(vc->queue);
err:
	return NULL;
}

/* Virtual Card exit-route */
void vc_exit(struct vc_node *vc)
{
	/* Free buffer */
	free(vc->RBuf);
	free(vc->WBuf);

	/* Remove TUN */
	close(vc->tun_fd);

	/* Remove queue */
	queue_exit(vc->queue);
	printf("\nSafe exit virtual card!\n");
}
