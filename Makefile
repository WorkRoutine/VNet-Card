MYCFLAGS = -g -O0 -Wall -fno-strict-aliasing -I./ -lpthread

all: Vnet-card_X86 Vnet-card_FPGA

%.o: %.c
	$(CC) -c -o $@ $< $(MYCFLAGS)

Vnet-card_X86: tap.o dma.o Vnet-card_X86.o fifo.o
	@rm -f $@
	$(CC) $^ -o $@ $(MYCFLAGS) $(MYLDFLAGS)

Vnet-card_FPGA: tap.o dma.o Vnet-card_FPGA.o fifo.o mem.o
	@rm -f $@
	$(CC) $^ -o $@ $(MYCFLAGS) $(MYLDFLAGS)

clean:
	$(RM) *.o Vnet-card_X86 Vnet-card_FPGA

