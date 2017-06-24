.PHONY: all clean

CC := gcc
CFLAGS := -Wall -O3  -Wmissing-prototypes -Wstrict-prototypes -Werror=implicit-function-declaration -Werror=format -mtune=native
LDFLAGS :=

OBJS := fast_crc32.o

all: fast_crc32

clean:
	rm -f $(OBJS) fast_crc32

test: all
	./fast_crc32 testfile

fast_crc32: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
