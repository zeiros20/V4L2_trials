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

#define FRAMES 10

int main(void){
    Buffer * buffer = {0};
    int fd = open(DEVICE, O_RDWR);
    device_desc(fd);
    set_format(fd, 1280,720,PIXEL_FMT);
    int buffer_cnt = init_buffer(fd,&buffer);
    start_capture(fd,buffer_cnt);
    monitor_fd(fd);
    //get_frame(fd,buffer_cnt);
    save_image(fd,buffer,FRAMES,buffer_cnt);
    stop_capture(fd);
}