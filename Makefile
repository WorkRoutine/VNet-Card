MYCFLAGS = -g -O0 -Wall -fno-strict-aliasing -I./

SRCS = tun-demo.c
all:tap-demo

%.o: %.c
	$(CC) -c -o $@ $< $(MYCFLAGS)

tap-demo: tap.o dma.o tap-demo.o fifo.o mem.o
	@rm -f $@
	$(CC) $^ -o $@ $(MYCFLAGS) $(MYLDFLAGS)

clean:
	$(RM) *.o tap-demo

