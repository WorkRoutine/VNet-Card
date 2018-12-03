#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <malloc.h>

#include "dma.h"

void *xdma_open(void)
{
    xdma_t *pXdma = (xdma_t *)malloc(sizeof(xdma_t));

    if(NULL==pXdma) 
        return NULL;
    memset(pXdma, 0, sizeof(xdma_t));

    pXdma->wFd = open(H2C0_CH0, O_RDWR);
    pXdma->rFd = open(C2H0_CH0, O_RDWR | O_NONBLOCK);
    if((pXdma->wFd<0) || (pXdma->rFd<0))
        goto ERROR;

    pXdma->wPhyAddr = WRITE_PHY_ADDR;
    pXdma->rPhyAddr = READ_PHY_ADDR;

    return (xHdl)pXdma;

ERROR:
    if(pXdma) {
        if(pXdma->wFd>0) {
            close(pXdma->wFd);
            pXdma->wFd = -1;
        }
        if(pXdma->rFd>0) {
            close(pXdma->rFd);
            pXdma->rFd = -1;
        }

        free(pXdma);
        pXdma = NULL;
    }
    return NULL;
}

static void pointer_reset(int fd, xHdl hdl, unsigned long offset)
{
    xdma_t *pXdma = (xdma_t *)hdl;
    uint32_t addr = (fd == pXdma->rFd) ? pXdma->rPhyAddr : pXdma->wPhyAddr;
	
    lseek(fd, addr + offset, SEEK_SET);
}

int xdma_write(uint8_t *pDate, uint32_t dataLen, unsigned long offset, 
                                              xHdl hdl)
{
    if(NULL==hdl)
        return -1;
    xdma_t *pXdma = (xdma_t *)hdl;

    pointer_reset(pXdma->wFd, pXdma, offset);
    if(write(pXdma->wFd, pDate, dataLen)!=dataLen) {
        printf("%s line:%d fail\n", __func__, __LINE__);
        return -1;
    }
    return 0;
}

int xdma_read(uint8_t *pDate, uint32_t dataLen, unsigned long offset, 
                                                         xHdl hdl)
{
    if(NULL==hdl)
        return -1;
    xdma_t *pXdma = (xdma_t *)hdl;

    pointer_reset(pXdma->rFd, pXdma, offset);
    if(read(pXdma->rFd, pDate, dataLen)!=dataLen) {
        printf("%s line:%d fail\n", __func__, __LINE__);
        return -1;
    }
    return 0;
}

void xdma_close(xHdl hdl)
{
    xdma_t *pXdma = (xdma_t *)hdl;
    if(NULL == hdl)
        return;

    close(pXdma->wFd);
    close(pXdma->rFd);
    free(hdl);
}

uint8_t *align_alloc(uint32_t size)
{
    void *p = NULL;
  
    posix_memalign((void **)&p, 4096/*alignment*/, size + 4096);
    assert(p);
    return (uint8_t *)p;
}
