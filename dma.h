#ifndef __APP_H__
#define __APP_H__

#include "fifo.h"

#define H2C0_CH0                "/dev/xdma0_h2c_0"
#define C2H0_CH0                "/dev/xdma0_c2h_0"

#define WRITE_PHY_ADDR		(BASE_PHY_ADDR)
#define READ_PHY_ADDR		(BASE_PHY_ADDR)
#define BUFFER_SIZE		(FIFO_BUFFER)

typedef void * xHdl;
typedef struct
{
    int wFd;
    int rFd;
    uint32_t wPhyAddr;
    uint32_t rPhyAddr;
} xdma_t;

extern void *xdma_open(void);
extern int xdma_write(uint8_t *pDate, uint32_t dataLen, unsigned long offset,
                                              xHdl hdl);
extern int xdma_read(uint8_t *pDate, uint32_t dataLen, unsigned long offset,
                                                         xHdl hdl);
extern void xdma_close(xHdl hdl);
extern uint8_t *align_alloc(uint32_t size);

#endif
