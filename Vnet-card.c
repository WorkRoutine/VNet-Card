/*
 * X86 Monitor
 *
 * 2018.12.03 BuddyZhang1 <buddy.zhang@aliyun.com>
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <errno.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <malloc.h>

#include "dma.h"
#include "tap.h"
#include "fifo.h"

/*
 * +---------------------+----------------------------------------+
 * |                     |                                        |
 * |        Flags        |                  Memory                |
 * |                     |                                        |
 * +---------------------+----------------------------------------+
 *
 */

static xHdl hdl;
/* Reload fifo manage information */

#define fifo_manage_get(base, offset) \
    if (xdma_read((uint8_t *)base, RESERVED_SIZES, offset, hdl)) { \
        printf("ERROR: XDMA Read!\n"); \
    }

#define fifo_manage_reload(base, offset) \
    if (xdma_read((uint8_t *)base, RESERVED_SIZES, offset, hdl)) { \
        printf("ERROR: XDMA Read!\n"); \
    } \
    reload_fifo((unsigned long)base);

#define fifo_manage_sync(base, offset) \
    if (xdma_write((uint8_t *)base, RESERVED_SIZES, offset, hdl)) { \
        printf("ERROR: XDMA write!\n"); \
    }

#if 0
/* Send procedure */
void *send_procedure(void *arg)
{
    unsigned char *base;

    /*
     * X86 initialize FIFO (Write data from X86 to FPGA)
     */
    base = (unsigned char *)align_alloc(RESERVED_SIZES);
    fifo_manage_get(base, X86_WR_FIFO_OFFSET);

    /* Initialize fifo */
    fifo_init((unsigned long)base, X86_WR_FIFO_OFFSET);
    if (InitLinkQueue() < 0) {
        printf("ERROR: Unable to init FIFO queue.\n");
        goto err;
    }
    /* sync */
    fifo_manage_sync(base);

    pthread_exit((void *)0);
}

/* Rece procedure */
void *recv_procedure(void *arg)
{

    pthread_exit((void *)0);
}
#endif

int main(int argc, char *argv[])
{
    char tun_name[IFNAMSIZ];
//    pthread_t sendt, recvt;
    unsigned char *Wbase;
    unsigned char *Rbase;
    unsigned char *WpBuf;
    unsigned char *RpBuf;
    int tun_fd;

    /*
     * Initialize TUN device....
     */
    tun_name[0] = '\0';
    /* IFF_TAP: layer 2; IFF_TUN: layer 3 */
    tun_fd = tun_create(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) {
        goto err_tun;
    }

    /*
     * Initialize DMA, create two buffer which used to read and write data.
     */
    WpBuf = align_alloc(BUFFER_SIZE);
    RpBuf = align_alloc(BUFFER_SIZE);
    if (!WpBuf || !RpBuf) {
        printf("ERROR: Unable to allocate memory from DMA.\n");
        goto err_alloc_DMA;
    }
    memset(RpBuf, 0x00, BUFFER_SIZE);
    memset(WpBuf, 0x00, BUFFER_SIZE);

    /* Open DMA */
    hdl = xdma_open();
    if(NULL == hdl) {
        printf("ERROR: Unable to open DMA Channel!\n");
        goto err_dma;
    }

#if 0
    /*
     * Establish two thread to send and receive data.
     */
    if (pthread_create(&sendt, NULL, send_procedure, NULL) != 0) {
        printf("ERROR: Unable to create send pthread.\n");
        goto err_pthd0;
    } else {
        if (pthread_join(sendt, NULL) != 0) {
            printf("ERROR: Unable to join send pthread.\n");
            goto err_join0;
        }
    }
    
    if (pthread_create(&recvt, NULL, recv_procedure, NULL) != 0) {
        printf("ERROR: Unable to create recv pthread.\n");
        goto err_pthd1;
    } else {
        if (pthread_join(recvt, NULL) != 0) {
            printf("ERROR: Unable to join recv pthread.\n");
            goto err_join1;
        }
    }

    while (1);
#endif

    /*
     * X86 initialize FIFO.
     */
    Wbase = (unsigned char *)align_alloc(RESERVED_SIZES);
    Rbase = (unsigned char *)align_alloc(RESERVED_SIZES);
    if (!Wbase || !Rbase)
        return -1;

    fifo_manage_get(Wbase, X86_WR_FIFO_OFFSET);
    fifo_manage_get(Rbase, X86_RD_FIFO_OFFSET);

    /* X86 initialize DMA-Write FIFO */
    fifo_init((unsigned long)Wbase);
    if (InitLinkQueue((unsigned long)Wbase) < 0) {
        printf("ERROR: Unable to init FIFO queue.\n");
        goto err;
    }
    /* sync */
    fifo_manage_sync((unsigned long)Wbase, X86_WR_FIFO_OFFSET);

    while (1) {
        unsigned long start, size, count;

        /* Read from Tun */
        count = read(tun_fd, WpBuf, 1200);

        fifo_manage_reload((unsigned long)Wbase, X86_WR_FIFO_OFFSET);
        /* Read fifo information */
        GetHeadElement((unsigned long)Wbase, &start, &size);

        /* Over MEM buffer */
        if ((start + size + count) > MEM_SIZES) {
            xdma_write(WpBuf, count, X86_WR_FIFO_OFFSET + MEM_OFFSET, hdl);
            PushElement((unsigned long)Wbase, 0, count);
        } else {
            xdma_write(WpBuf, count, 
                 X86_WR_FIFO_OFFSET + MEM_OFFSET + start + size, hdl);
            PushElement((unsigned long)Wbase, start + size, count);
        }
        fifo_manage_sync(Wbase, X86_WR_FIFO_OFFSET);
    }

    return 0;

err:

err_dma:

err_alloc_DMA:
    tun_release(tun_fd);
err_tun:

    return -1;

}
