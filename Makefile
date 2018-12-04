MYCFLAGS = -g -O0 -Wall -fno-strict-aliasing -I./ -lpthread

SRCS = tun-demo.c
all: Vnet-card

%.o: %.c
	$(CC) -c -o $@ $< $(MYCFLAGS)

Vnet-card: tap.o dma.o Vnet-card.o fifo.o mem.o
	@rm -f $@
	$(CC) $^ -o $@ $(MYCFLAGS) $(MYLDFLAGS)

clean:
	$(RM) *.o Vnet-card

