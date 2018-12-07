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
#include "dma.h"

static int tun_fd;

static unsigned char *Wbase;
static unsigned char *Rbase;
static unsigned char *WpBuf;
static unsigned char *RpBuf;
static unsigned char *mBuf;

/* Reload fifo manage information */
#define EFLAGS_BUSY            (1 << 0)
#define EFLAGS_READ            (1 << 1)
#define EFLAGS_WRITE           (1 << 2)

#define fifo_manage_get(base, offset) \
    memcpy((unsigned char *)base + MAGIC_BYTES, (unsigned char *)mBuf + \
                       offset + MAGIC_BYTES, RESERVED_SIZES)

#define fifo_manage_sync(base, offset) \
    memcpy((unsigned char *)mBuf + offset + MAGIC_BYTES, \
             (unsigned char *)base + MAGIC_BYTES, RESERVED_SIZES)

#define fifo_eflags_get(base, offset) \
    memcpy(base, (unsigned char *)mBuf + offset + EFLAGS_OFFSET, EFLAGS_BYTES)

#define fifo_eflags_put(base, offset) \
    memcpy((unsigned char *)mBuf + offset + EFLAGS_OFFSET, base, EFLAGS_BYTES)

static inline unsigned int fifo_lock(unsigned long offset)
{
    unsigned int eflags;

    fifo_eflags_get(&eflags, offset);

    eflags |= EFLAGS_BUSY;

    fifo_eflags_put(&eflags, offset);
   
    return eflags;
}

static inline unsigned int fifo_unlock(unsigned int eflags,
                                                  unsigned long offset)
{
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

static inline int wait_and_ready(unsigned long offset)
{
    unsigned int eflags;

    while (1) {
        fifo_eflags_get(&eflags, offset);

        if ((eflags & EFLAGS_WRITE) != EFLAGS_WRITE)
            continue;

        if ((eflags & EFLAGS_BUSY) != EFLAGS_BUSY)
            break;
    }
    eflags |= EFLAGS_BUSY;
    fifo_eflags_put(&eflags, offset);

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
    unsigned int eflags = 0;
    unsigned long total = 0;

    /*
     * Create Write buffer.
     */
    WpBuf = align_alloc(FIFO_BUFFER);
    if (!WpBuf) {
        printf("ERROR: Unable to allocate memory from DMA.\n");
        return NULL;
    }
    memset(WpBuf, 0x00, FIFO_BUFFER);

    /* 
     * FPGA initialize FIFO
     */
    Wbase = align_alloc(RESERVED_SIZES);
    if (!Wbase)
        return NULL;

    fifo_manage_get(Wbase, ARM_WR_FIFO_OFFSET);

    /* FPGA initialize Write FIFO */
    fifo_init((unsigned long)Wbase);
    fifo_eflags_put(&eflags, ARM_WR_FIFO_OFFSET);
    if (InitLinkQueue((unsigned long)Wbase) < 0) {
        printf("ERROR: Unable to init FIFO queue.\n");
        return NULL;
    }

    /* sync */
    fifo_manage_sync((unsigned long)Wbase, ARM_WR_FIFO_OFFSET);

    while (1) {
        unsigned long start, size, count;

        /* Read from Tun/Tap */
        count = read(tun_fd, WpBuf, 1200);

        /* Set Vnet-card Ready */
        eflags = wait_and_lock(ARM_WR_FIFO_OFFSET);
        fifo_manage_get(Wbase, ARM_WR_FIFO_OFFSET);
        if ((eflags & EFLAGS_READ) == EFLAGS_READ) {
            /* flush out all fifo */
            while (!IsQueueEmpty((unsigned long)Wbase))
                PopElement((unsigned long)Wbase, &start, &size);
            /* Clear R bit */
            eflags &= ~EFLAGS_READ;
        }

        /* Read fifo information */
        GetHeadElement((unsigned long)Wbase, &start, &size);
        total += size;

        /* Over MEM buffer */
        if ((total + count) > MEM_SIZES) {
            memcpy((unsigned char *)mBuf + MEM_OFFSET + ARM_WR_FIFO_OFFSET, 
                                                   WpBuf, count);
            PushElement((unsigned long)Wbase, 0, count);
            total = 0;
        } else {
            memcpy((unsigned char *)mBuf + MEM_OFFSET + ARM_WR_FIFO_OFFSET + 
                         start + size, WpBuf, count);
            PushElement((unsigned long)Wbase, total, count);
        }

        fifo_manage_sync(Wbase, ARM_WR_FIFO_OFFSET);
        eflags |= EFLAGS_WRITE;
        fifo_unlock(eflags, ARM_WR_FIFO_OFFSET);
    }

    return NULL;
}

/* Rece procedure */
void *recv_procedure(void *arg)
{
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
    Rbase = malloc(RESERVED_SIZES);
    if (!Rbase)
        return NULL;

    /* Read-monitor */
    while (1) {
        unsigned int *magic;
        unsigned long base, size;
        unsigned int eflags;

        eflags = wait_and_ready(ARM_WR_FIFO_OFFSET);
        if ((eflags & EFLAGS_READ) == EFLAGS_READ) {
            /* No write data! */
            fifo_unlock(eflags, ARM_WR_FIFO_OFFSET);
            usleep(2000);
            continue;
        }
        /* Set R bit */
        eflags |= EFLAGS_READ;
        eflags &= ~EFLAGS_WRITE;
        /* Obtain newest read-fifo information */
        fifo_manage_get(Rbase, ARM_WR_FIFO_OFFSET);
        fifo_unlock(eflags, ARM_WR_FIFO_OFFSET);
        magic = (unsigned int *)(Rbase + MAGIC_OFFSET);

        if (*magic != FIFO_MAGIC)
            continue;

        if (IsQueueEmpty((unsigned long)Rbase))
            continue;

        /* Read from FIFO */
        while (!IsQueueEmpty((unsigned long)Rbase)) {
            unsigned char ip[4];

            PopElement((unsigned long)Rbase, &base, &size);

            memcpy(RpBuf, (unsigned char *)mBuf + MEM_OFFSET + 
                                    ARM_WR_FIFO_OFFSET + base, size);
            
            memcpy(ip, &RpBuf[12], 4);
            memcpy(&RpBuf[12], &RpBuf[16], 4);
            memcpy(&RpBuf[16], ip, 4);

            RpBuf[20] = 0;

            *((unsigned short *)&RpBuf[22]) += 8;
            write(tun_fd, RpBuf, size);
        }
        /* Only*/
    }

    return NULL;
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
