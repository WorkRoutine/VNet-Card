/*
 * X86 Monitor
 *
 * 2018.12.03 BuddyZhang1 <buddy.zhang@aliyun.com>
 */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
#include <sys/types.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>

#include "dma.h"
#include "tap.h"
#include "fifo.h"

static xHdl hdl;
static int tun_fd;

static unsigned char *Wbase;
static unsigned char *Rbase;
static unsigned char *WpBuf;
static unsigned char *RpBuf;

/* Reload fifo manage information */
#define EFLAGS_BUSY            (1 << 0)
#define EFLAGS_READ            (1 << 1)

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

#define fifo_eflags_get(base, offset) \
    if (xdma_read((uint8_t *)base, EFLAGS_BYTES, offset + EFLAGS_OFFSET, hdl)) { \
        printf("ERROR: XDMA Read!\n"); \
    }

#define fifo_eflags_put(base, offset) \
    if (xdma_write((uint8_t *)base, EFLAGS_BYTES, offset + EFLAGS_OFFSET, hdl)) { \
        printf("ERROR: XDMA Write!\n"); \
    }
    
static inline unsigned int fifo_lock(unsigned long offset)
{
    unsigned int eflags;

    fifo_eflags_get(&eflags, offset);

    eflags |= EFLAGS_BUSY;

    fifo_eflags_put(&eflags, offset);

    return eflags;
}

static inline unsigned int fifo_unlock(unsigned long offset)
{
    unsigned int eflags;

    fifo_eflags_get(&eflags, offset);

    eflags &= ~EFLAGS_BUSY;

    fifo_eflags_put(&eflags, offset);

    return eflags;
}

static inline int wait_lock(unsigned long offset)
{
    unsigned int eflags;

    while (1) {
        fifo_eflags_get(&eflags, offset);

        if ((eflags & EFLAGS_BUSY) != EFLAGS_BUSY)
            break;
    }
    return eflags;
}

static inline int wait_and_lock(unsigned long offset)
{
    unsigned int eflags;

    while (1) {
        fifo_eflags_get(&eflags, offset);

        if ((eflags & EFLAGS_BUSY) != EFLAGS_BUSY)
            break;
    }
    eflags |= EFLAGS_BUSY;
    fifo_eflags_put(&eflags, offset);
    return eflags;
}

/* Send procedure */
void *send_procedure(void *arg)
{
    /*
     * Initialize DMA, create Write buffer.
     */
    WpBuf = align_alloc(BUFFER_SIZE);
    if (!WpBuf) {
        printf("ERROR: Unable to allocate memory from DMA.\n");
        return NULL;
    }
    memset(WpBuf, 0x00, BUFFER_SIZE);

    /*
     * X86 initialize FIFO.
     */
    Wbase = (unsigned char *)align_alloc(RESERVED_SIZES);
    if (!Wbase)
        return NULL;

    fifo_manage_get(Wbase, X86_WR_FIFO_OFFSET);

    /* X86 initialize DMA-Write FIFO */
    fifo_init((unsigned long)Wbase);
    if (InitLinkQueue((unsigned long)Wbase) < 0) {
        printf("ERROR: Unable to init FIFO queue.\n");
        return NULL;
    }
    /* sync */
    fifo_manage_sync((unsigned long)Wbase, X86_WR_FIFO_OFFSET);

    while (1) {
        unsigned long start, size, count;
        unsigned int eflags;

        /* Read from Tun/Tap */
        count = read(tun_fd, WpBuf, 1200);

        eflags = wait_and_lock(X86_WR_FIFO_OFFSET);
        fifo_manage_get(Wbase, X86_WR_FIFO_OFFSET);
        if ((eflags & EFLAGS_READ) == EFLAGS_READ) {
            /* flush out all fifo */
            while (!IsQueueEmpty((unsigned long)Wbase))
                PopElement((unsigned long)Wbase, &start, &size);
            /* Clear R bit */
            eflags &= ~EFLAGS_READ;
            fifo_eflags_put(&eflags, X86_WR_FIFO_OFFSET);
        }

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
        eflags = fifo_unlock(X86_WR_FIFO_OFFSET);
    }

    return NULL;
}

/* Rece procedure */
void *recv_procedure(void *arg)
{
    /*
     * Initialize DMA, create Write buffer.
     */
    RpBuf = align_alloc(BUFFER_SIZE);
    if (!RpBuf) {
        printf("ERROR: Unable to allocate memory from DMA.\n");
        return NULL;
    }
    memset(RpBuf, 0x00, BUFFER_SIZE);

    /*
     * X86 initialize FIFO.
     */
    Rbase = (unsigned char *)align_alloc(RESERVED_SIZES);
    if (!Rbase)
        return NULL;

    /* Read-monitor */
    while (1) {
        unsigned int *magic;
        unsigned long base, size;
        unsigned int eflags;

        sleep(1);
        eflags = wait_and_lock(X86_WR_FIFO_OFFSET);
        /* Set R bit */
        eflags |= EFLAGS_READ;
        fifo_eflags_put(&eflags, X86_WR_FIFO_OFFSET);
        /* Obtian newest read-fifo information */
        fifo_manage_get(Rbase, X86_WR_FIFO_OFFSET);
        fifo_unlock(X86_WR_FIFO_OFFSET);
        magic = (unsigned int *)(Rbase + MAGIC_OFFSET);

        if (*magic != FIFO_MAGIC) 
            continue;

        if (IsQueueEmpty((unsigned long)Rbase))
            continue;

        /* Read from FIFO */
        while (!IsQueueEmpty((unsigned long)Rbase)) {
            unsigned char ip[4];

            PopElement((unsigned long)Rbase, &base, &size);

            xdma_read(RpBuf, size, X86_WR_FIFO_OFFSET + MEM_OFFSET + base, hdl);

            memcpy(ip, &RpBuf[12], 4);
            memcpy(&RpBuf[12], &RpBuf[16], 4);
            memcpy(&RpBuf[16], ip, 4);

            RpBuf[20] = 0;

            *((unsigned short *)&RpBuf[22]) += 8;
            write(tun_fd, RpBuf, size);
        }
    }

    return NULL;
}

/* Handle signal from system */
static do_signal(int sig)
{
    switch (sig) {
    case SIGSTOP:
        /* CTRL Z */
    case SIGINT:
        /* CTRL C */
    case SIGQUIT:
        /* CTRL \ */
    case SIGTERM:
        /* Termnal */
    case SIGKILL:
        /* Kill -9 */
        /* Release READ/Write Manage */
        fifo_release((unsigned long)Wbase);
        fifo_manage_sync((unsigned long)Wbase, X86_WR_FIFO_OFFSET);
        free(Wbase);
        free(WpBuf);
        free(Rbase);
        free(RpBuf);
        printf("Un-normal-exit.\n");
        exit (1);
        break;
    default:
        fprintf(stderr, "Undefined signal.\n");
        exit (1);
    }
}

/* Initialize all signal */
void signal_init(void)
{
    signal(SIGSTOP, do_signal);
    signal(SIGINT,  do_signal);
    signal(SIGQUIT, do_signal);
    signal(SIGKILL, do_signal);
    signal(SIGTERM, do_signal);
}

int main(int argc, char *argv[])
{
    char tun_name[IFNAMSIZ];
    pthread_t sendt, recvt;

    /*
     * Initialize TUN device....
     */
    tun_name[0] = '\0';
    /* IFF_TAP: layer 2; IFF_TUN: layer 3 */
    tun_fd = tun_create(tun_name, IFF_TUN | IFF_NO_PI, "10.10.10.1");
    if (tun_fd < 0) {
        return -1;
    }

    /* Open DMA */
    hdl = xdma_open();
    if(NULL == hdl) {
        printf("ERROR: Unable to open DMA Channel!\n");
        return -1;
    }

    /* Initialize signation */
    signal_init();

    /*
     * Establish two thread to send and receive data.
     */
    if (pthread_create(&sendt, NULL, send_procedure, NULL) != 0) {
        printf("ERROR: Unable to create send pthread.\n");
        return -1;
    } 
    if (pthread_create(&recvt, NULL, recv_procedure, NULL) != 0) {
        printf("ERROR: Unable to create recv pthread.\n");
        return -1;
    }
    if (pthread_join(sendt, NULL) != 0) {
        printf("ERROR: Unable to join send pthread.\n");
        return -1;
    }
    
    if (pthread_join(recvt, NULL) != 0) {
        printf("ERROR: Unable to join recv pthread.\n");
        return -1;
    }

    return 0;
}
