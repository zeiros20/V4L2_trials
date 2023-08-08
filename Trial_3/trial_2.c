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

#define FRAMES 100

int main(int argc, char* argv[]){
    if(argc < 5){
        perror("INVALID INPUT");
        exit(1);
    }
    Buffer * buffer = {0};
    const char*  DEVICE = argv[1];
    long width = strtol(argv[2], NULL,10) , height = strtol(argv[3], NULL, 10), frames = strtol(argv[4],NULL,10);
    int fd = open(DEVICE, O_RDWR);
    device_desc(fd);
    set_format(fd,width,height,PIXEL_FMT);
    int buffer_cnt = init_buffer(fd,&buffer);
    start_capture(fd,buffer_cnt);
    monitor_fd(fd);
    //get_frame(fd,buffer_cnt);
    save_image(fd,buffer,frames,buffer_cnt);
    stop_capture(fd);
    return 0;
}