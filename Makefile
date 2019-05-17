
HOST_CC=gcc
CROSS_CC=/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-newest-arm64/aarch64-linux-gnu/aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc

## CFLAGS
CFLAGS += -I./ -lpthread

CONFIG_ALL :=
SRC := main.c base.c signal.c

# CONFIG_DMA_QUEUE
# 
SRC        += queue.c tap_tun.c 

# CONFIG_

all: Vnet_Host Vnet_FPGA

Vnet_Host: $(SRC)
	@$(HOST_CC) $(SRC) $(CFLAGS) -DCONFIG_HOST $(CONFIG_ALL) -o $@

Vnet_FPGA: $(SRC)
	@$(CROSS_CC) $(SRC) $(CFLAGS) -DCONFIG_FPGA $(CONFIG_ALL) -o $@

clean:
	@rm -rf Vnet_*
