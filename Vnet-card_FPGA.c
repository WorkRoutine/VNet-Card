/*
 * FPGA Monitor
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

#include "tap.h"
#include "fifo.h"
#include "mem.h"

static int tun_fd;

static unsigned char *Wbase;
static unsigned char *Rbase;
static unsigned char *WpBuf;
static unsigned char *RpBuf;
static unsigned char *mBuf;

/* Reload fifo manage information */
#define EFLAGS_BUSY            (1 << 0)
#define EFLAGS_READ            (1 << 1)

static inline unsigned int fifo_lock(unsigned int *base)
{
    unsigned int eflags = *base;

    eflags |= EFLAGS_BUSY;

    *base = eflags;
   
    return eflags;
}

static inline unsigned int fifo_unlock(unsigned int *base)
{
    unsigned int eflags = *base;

    eflags &= ~EFLAGS_BUSY;

    *base = eflags;

    return eflags;
}

static inline int wait_lock(unsigned int *base)
{
    unsigned int eflags = *base;

    while (1) {
        if ((eflags & EFLAGS_BUSY) != EFLAGS_BUSY)
            break;
    }
    return eflags;
}

static inline int wait_and_lock(unsigned int *base)
{
    unsigned int eflags = *base;

    while (1) {
        if ((eflags & EFLAGS_BUSY) != EFLAGS_BUSY)
            break;
    }
    eflags |= EFLAGS_BUSY;
    *base = eflags;

    return eflags;
}

/* Send procedure */
void *send_procedure(void *arg)
{
    /*
     * Create Write buffer.
     */
    WpBuf = malloc(FIFO_BUFFER);
    if (!WpBuf) {
        printf("ERROR: Unable to allocate memory from DMA.\n");
        return NULL;
    }
    memset(WpBuf, 0x00, FIFO_BUFFER);

    Wbase = mBuf + ARM_WR_FIFO_OFFSET;

    /* FPGA initialize Write FIFO */
    fifo_init((unsigned long)Wbase);
    if (InitLinkQueue((unsigned long)Wbase) < 0) {
        printf("ERROR: Unable to init FIFO queue.\n");
        return NULL;
    }

    while (1) {
        unsigned long start, size, count;
        unsigned int eflags;

        /* Read from Tun/Tap */
        count = read(tun_fd, WpBuf, 1200);

        eflags = wait_and_lock((unsigned int *)Wbase);
        if ((eflags & EFLAGS_READ) == EFLAGS_READ) {
            /* flush out all fifo */
            while (!IsQueueEmpty((unsigned long)Wbase))
                PopElement((unsigned long)Wbase, &start, &size);
            /* Clear R bit */
            eflags &= ~EFLAGS_READ;
            *(unsigned int *)Wbase = eflags;
        }

        /* Read fifo information */
        GetHeadElement((unsigned long)Wbase, &start, &size);

        /* Over MEM buffer */
        if ((start + size + count) > MEM_SIZES) {
            memcpy((unsigned char *)Wbase + MEM_OFFSET, WpBuf, count);
            PushElement((unsigned long)Wbase, 0, count);
        } else {
            memcpy((unsigned char *)Wbase + MEM_OFFSET + start + size, 
                                            WpBuf, count);
            PushElement((unsigned long)Wbase, start + size, count);
        }
        eflags = fifo_unlock((unsigned int *)Wbase);
    }

    return NULL;
}

/* Rece procedure */
void *recv_procedure(void *arg)
{
    unsigned int *magic;

    /*
     * Initialize DMA, create Write buffer.
     */
    RpBuf = malloc(FIFO_BUFFER);
    if (!RpBuf) {
        printf("ERROR: Unable to allocate memory from DMA.\n");
        return NULL;
    }
    memset(RpBuf, 0x00, FIFO_BUFFER);

    /*
     * X86 initialize FIFO.
     */
    Rbase = mBuf + ARM_WR_FIFO_OFFSET;

    /* Read-monitor */
    while (1) {
        unsigned long base, size;
        unsigned int eflags;

        sleep(1);
        eflags = wait_and_lock((unsigned int *)Rbase);
        /* Set R bit */
        eflags |= EFLAGS_READ;
        *(unsigned int *)Rbase = eflags;
        fifo_unlock((unsigned int *)Rbase);

        magic = (unsigned int *)(Rbase + MAGIC_OFFSET);
        while (1) {
            if ((unsigned int)*magic == FIFO_MAGIC)
                break;
            sleep(1);
        }

        if (IsQueueEmpty((unsigned long)Rbase))
            continue;

        /* Read from FIFO */
        while (!IsQueueEmpty((unsigned long)Rbase)) {
            unsigned char ip[4];

            PopElement((unsigned long)Rbase, &base, &size);

            memcpy(RpBuf, (unsigned char *)Rbase + MEM_OFFSET + base, size);

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
    tun_fd = tun_create(tun_name, IFF_TUN | IFF_NO_PI, "10.10.10.5");
    if (tun_fd < 0) {
        return -1;
    }

    /* Initialize signation */
    //signal_init();

    if (!(mBuf = mem_init(BASE_PHY_ADDR, TOTAL_SIZES * 2))) {
        printf("ERROR: Unable allocate memory to PHY\n");
        return -1;
    }

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
