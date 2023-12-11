CFLAGS = -O0 -g3 -ggdb 

all:matfs.c
	gcc $(CFLAGS) -o matfs matfs.c

run:matfs
	./matfs /dev/nvme0n1p3

debug:matfs
	sudo cgdb -x gdb.init