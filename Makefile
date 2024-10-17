
TESTS = u-dma-buf-test        \
        u-dma-buf-direct-test \
        u-dma-buf-ioctl-test  \
        u-dma-buf-file-test   \
        $(END_LIST)

DELETE     := -rm
CC         := gcc
CC_OPTIONS := -g

all: $(TESTS)

clean:
	$(DELETE) $(TESTS)

u-dma-buf-test: u-dma-buf-test.c
	$(CC) $(CC_OPTIONS) -o $@ $<

u-dma-buf-direct-test: u-dma-buf-direct-test.c
	$(CC) $(CC_OPTIONS) -o $@ $<

u-dma-buf-ioctl-test: u-dma-buf-ioctl-test.c
	$(CC) $(CC_OPTIONS) -o $@ $<

u-dma-buf-file-test: u-dma-buf-file-test.c
	$(CC) $(CC_OPTIONS) -o $@ $<

u-dma-buf-uring-test: u-dma-buf-uring-test.c
	$(CC) $(CC_OPTIONS) -luring -o $@ $<

