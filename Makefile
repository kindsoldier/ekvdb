#
#  Copyright 2023 Oleg Borodin  <borodin@unix7.org>
#

all: test

CC = cc
CFLAGS = -O -Wall -I. -std=c99 -pthread
LDFLAGS = -pthread

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

hwmemory.c: hwmemory.h
hwmemory.o: hwmemory.c

hwstore.c: hwstore.h
hwstore.o: hwstore.c

hwstore_test.c: hwstore.h
hwstore_test.o: hwstore_test.c

OBJS += hwstore.o
OBJS += hwmemory.o

hwstore_test: hwstore_test.o $(OBJS)
	$(CC) $(LDFLAGS) -o $@ hwstore_test.o $(OBJS)

test: hwstore_test
	./hwstore_test

clean:
	rm -f *_test
	rm -f *.o *~

#EOF
