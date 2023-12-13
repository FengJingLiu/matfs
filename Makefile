all:mkfs

mkfs:mkfs.matfs.c
	gcc -o mkfs.matfs mkfs.matfs.c

clean:
	rm mkfs.matfs