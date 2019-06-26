# SPDX-License-Identifier: GPL-2.0 
HOST_CC=gcc
CROSS_CC=

## CFLAGS
CFLAGS += -I./ -lpthread -g

## Configuration
# CONFIG_ALL += -DCONFIG_SOCKET_DEBUG
# CONFIG_ALL += -DCONFIG_MSG_PARSE

SRC := main.c base.c signal.c

# CONFIG_DMA_QUEUE
# 
SRC        += queue.c tap_tun.c dma.c

all: Vnet_Host Vnet_FPGA

Vnet_Host: $(SRC)
	@$(HOST_CC) $(SRC) $(CFLAGS) -DCONFIG_HOST $(CONFIG_ALL) -o $@

Vnet_FPGA: $(SRC)
	@$(CROSS_CC) $(SRC) $(CFLAGS) -DCONFIG_FPGA $(CONFIG_ALL) -o $@

clean:
	@rm -rf Vnet_*
