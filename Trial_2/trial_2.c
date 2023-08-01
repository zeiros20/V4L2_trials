#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "interface.h"

int main(void){
    int fd = open(DEVICE, O_RDWR);
    device_desc(fd);
    set_formate(fd, 1280,720,PIXEL_FMT);
}