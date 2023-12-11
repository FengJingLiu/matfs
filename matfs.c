#include <fcntl.h>
#include <linux/nvme_ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int
oper_posix(const char* filename)
{
    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    char* buffer = malloc(4096 * 1024);
    if (!buffer) {
        goto release;
        perror("malloc");
        return 1;
    }

    memset(buffer, 0, 4096 * 1024);

#if 0
    strcpy(buffer, "QWERTYUIOPASDFGHJKLZXCVBNM");

    int ret = write(fd, buffer, strlen(buffer));
    if (ret < 0) {
        perror("write");
        goto release;
        return 1;
    }

    printf("write succesful\n");
    close(fd);

    fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open");
        goto release;
        return 1;
    }
    memset(buffer, 0, 4096);
#endif
    int ret = read(fd, buffer, 4096 * 1024);
    if (ret < 0) {
        perror("read");
        goto release;
        return 1;
    }

    for (int i = 0; i < 4096 * 1024; ++i) {
        if (i % 128 == 0) {
            printf("\n%c", buffer[i]);
        } else {
            printf("%c", buffer[i]);
        }
    }

release:
    free(buffer);
    close(fd);
    return 0;
}

int
oper_nvme(const char* filename)
{
    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    char* buffer = malloc(4096);
    if (!buffer) {
        perror("malloc");
        goto release;
        return 1;
    }

    memset(buffer, 0, 4096);
    strcpy(buffer, "0QWERTYUIOPASDFGHJKLZXCVBNM");

    struct nvme_user_io io;
    memset(&io, 0, sizeof(struct nvme_user_io));
    io.addr    = (__u64)buffer;
    io.slba    = 0;
    io.nblocks = 1;
    io.opcode  = 1;

    if (-1 == ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io)) {
        perror("ioctl write");
        goto release;
    }
    printf("write succesful\n");

    memset(buffer, 0, 4096);
    io.opcode = 2;
    if (-1 == ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io)) {
        perror("ioctl read");
        goto release;
    }
    printf("read succesful\n");
    printf("read %s\n", buffer);

release:
    free(buffer);
    close(fd);

    return 0;
}

int
main(int argc, char* argv[])
{
    if (argc < 2) {
        perror("argc");
        return -1;
    }

    const char* filename = argv[1];
    oper_posix(filename);
    // oper_nvme(filename);
}