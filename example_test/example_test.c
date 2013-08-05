#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEVICE_FILE "/dev/example"

int main(int argc, char *argv[])
{
    int ret = 0, num = 12, val = 0, fd = 0;

    fd = open(DEVICE_FILE, O_RDWR);
    if(fd < 0)
    {
        printf("open device error!\n");
        return -1;
    }

    ret = write(fd, &num, sizeof(int));
    if(ret < 0)
    {
        printf("write device error!\n");
        return -1;
    }

    printf("write val %d to device!\n", num);

   ret = read(fd, &val, sizeof(int));
    if(ret < 0)
    {
        printf("read device error!\n");
        return -1;
    }

    printf("read val = %d\n", val);

    return 0;
}
