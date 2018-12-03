/*
 * X86 Monitor
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

int main(int argc, char *argv[])
{
    int ret;
    char tun_name[IFNAMSIZ];
    uint8_t *pBuf;
    xHdl hdl;
    unsigned char *base;
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
     * Initialize DMA....
     */
    pBuf = align_alloc(BUFFER_SIZE);
    if (NULL == pBuf) {
        printf("ERROR: Unable to allocate memory from DMA.\n");
        goto err_alloc_DMA;
    }
    memset(pBuf, 0x00, BUFFER_SIZE);

    /* Open DMA */
    hdl = xdma_open();
    if(NULL == hdl) {
        printf("ERROR: Unable to open DMA Channel!\n");
        goto err_dma;
    }

    /*
     * Initialize FIFO
     */
    base = (unsigned char *)align_alloc(RESERVED_SIZES);
    /* Obtian fifo manage information */ 
    if (xdma_read(base, RESERVED_SIZES, X86_TO_FPGA_OFFSET, hdl)) {
        printf("ERROR: XDMA Read!\n");
        goto err;
    }
    /* Initialize fifo */
    fifo_init((unsigned long)base);
    if (InitLinkQueue() < 0) {
        printf("ERROR: Unable to init FIFO queue.\n");
        goto err;
    }

    while (1) {
        unsigned long start, size;

        /* Read from Tun */
        ret = read(tun_fd, pBuf, 1200);
        /* reload fifo */
        if (xdma_read(base, RESERVED_SIZES, X86_TO_FPGA_OFFSET, hdl)) {
            printf("ERROR: XDMA Read!\n");
            goto err;
        }
        reload_fifo((unsigned long)base);

        /* Read fifo information */
        GetHeadElement(&start, &size);
        if (PushElement(start + size, ret) < 0) {
            printf("Unable push data into FIFO.\n");
            return -1;
        }
    }

    return 0;

err:

err_dma:

err_alloc_DMA:
    tun_release(tun_fd);
err_tun:

    return -1;

}
