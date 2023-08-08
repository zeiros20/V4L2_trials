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


static int xioctl(int fd, int request, void *arg){
    int sys_return;

    do{
        sys_return = ioctl(fd,request,arg);
    }while(sys_return == -1 && errno == EINTR);

    return sys_return;
}

void device_desc(int fd){
    struct v4l2_capability caps = {0};

    struct v4l2_cropcap crop_caps = {0};
    crop_caps.type = DATASTREAM;

    struct v4l2_fmtdesc fmt_desc = {0};
    fmt_desc.type = DATASTREAM;
    char c,e;

    if(xioctl(fd, VIDIOC_QUERYCAP,&caps) == -1){
        perror("Querying capabilities");
        exit(errno);
    }

    if(xioctl(fd, VIDIOC_CROPCAP,&crop_caps) == -1){
        perror("Querying crop capabilities");
        exit(errno);
    }
    // at this point caps,crop_caps and fmt_desc were modified and now holds the information gotten from the device
        printf("Driver Caps:\n"
        "  Driver: \"%s\"\n"
        "  Card: \"%s\"\n"
        "  Bus: \"%s\"\n"
        "  Version: %u.%u.%u\n"
        "  Capabilities: %08x\n",
        caps.driver,
        caps.card,
        caps.bus_info,
        (caps.version >> 16) & 0xFF,
        (caps.version >> 8) & 0xFF,
        (caps.version ) & 0XFF,
        caps.capabilities);
        
        printf("Camera Cropping:\n"
        "  Bounds: %dx%d+%d+%d\n"
        "  Default: %dx%d+%d+%d\n"
        "  Aspect: %d/%d\n",
        crop_caps.bounds.width, crop_caps.bounds.height, crop_caps.bounds.left, crop_caps.bounds.top,
        crop_caps.defrect.width, crop_caps.defrect.height,
    	crop_caps.defrect.left, crop_caps.defrect.top,
        crop_caps.pixelaspect.numerator, crop_caps.pixelaspect.denominator);

        while(xioctl(fd,VIDIOC_ENUM_FMT,&fmt_desc)==0){
            c = fmt_desc.flags & 1 ? 'C' : ' ';
            e = fmt_desc.flags & 2 ? 'E' : ' ';

            printf("[%d] %c%c %s\n",fmt_desc.index, c, e, fmt_desc.description);
            fmt_desc.index++;
        }


}

void set_format(int fd, int width, int height, int pixel_format){
    struct v4l2_format fmt = {0};
    fmt.type = DATASTREAM;
    
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if(xioctl(fd, VIDIOC_S_FMT,  &fmt) == -1){
        perror("Setting format");
        exit(errno);
    }

    char format_code[5];
    strncpy(format_code, (char*)&fmt.fmt.pix.pixelformat, 5);
    printf("Set format:\n"
	" Width: %d\n"
	" Height: %d\n"
	" Pixel format: %s\n"
	" Field: %d\n",
	fmt.fmt.pix.width,
	fmt.fmt.pix.height,
	format_code,
	fmt.fmt.pix.field);
}

int init_buffer(int fd, Buffer **buffer){
    
    // Buffer request from device
    struct v4l2_requestbuffers req_buf = {0};
    unsigned int i;
    memset(&req_buf,0,sizeof(req_buf));
    req_buf.type = DATASTREAM;
    req_buf.memory = MEMORY;
    req_buf.count = BUFFER_FRAMES;

    if(xioctl(fd,VIDIOC_REQBUFS,&req_buf) == -1){
        if(errno == EINVAL)
            printf("Video capturing or mmap is not supported");
        else
            perror("VIDIOC_REQBUFS");
        exit(EXIT_FAILURE);
    }

    if(req_buf.count < MIN_BUFFER_FRAMES){
        printf("Not enough buffer memory");
        exit(EXIT_FAILURE);
    }
    *buffer = calloc(req_buf.count, sizeof(**buffer));
    assert(*buffer != NULL);

    // buff is the device memory buffer 
    // each index is a frame from the buffer that was requested from the previous part
    struct v4l2_buffer buff;
    for(i=0; i<req_buf.count; i++){
        memset(&buff, 0, sizeof(buff));
        buff.type = DATASTREAM;
        buff.memory = MEMORY;
        buff.index =  i;

        // VIDIOC_QUERYBUF requests information from the buffer aka data
        if(xioctl(fd, VIDIOC_QUERYBUF, &buff) == -1){
            perror("Buffer querying");
            exit(errno);
        }
        (*(buffer)+i)->length = buff.length;
        //  mmap acts as memory allocation function
        //  put it simply mmap or memory mapping use pointers to exchange data without moving them from their main memory
        //  Think of it as
        /*   There are 2 type of memory or storing entities
                1)  user space memory
                2)  device main memory
            What happen here is, device gets data and put it in its memory
            by this data is stored within the device
            mmap allocate some space in user space at run time making a segment under the stack
            that segment will hold a pointer that points at the memory segment were the data is stored withing the device
        */
        
        (*(buffer)+i)->data = mmap(NULL,buff.length, PROT_READ | PROT_WRITE, MAP_SHARED,fd,buff.m.offset);
        if(MAP_FAILED == (*(buffer)+i)->data){
            perror("mmap failed");
            exit(EXIT_FAILURE);
        }
    }
    return req_buf.count;
} 


void start_capture(int fd, int buffer_cnt){
    enum v4l2_buf_type type;
    struct v4l2_buffer buff;

    // first thing to do is to start by enqueuing the device's buffer
    // using ioctl with VIDIOC_QBUF to send a request to enqueue the buffer
    for(int i = 0; i<buffer_cnt; i++){
        memset(&buff,0, sizeof(buff));
        buff.type = DATASTREAM;
        buff.memory = MEMORY;
        buff.index = i;
        if (xioctl(fd, VIDIOC_QBUF, &buff) == -1) {
            perror("Queue buffer at capture");
            exit(errno);
        }
    }
    // next step is to start streaming of the camera so that it starts capturing images
    type = DATASTREAM;
    if(xioctl(fd,VIDIOC_STREAMON, &type)==-1){
        perror("Stream ON");
        exit(errno);
    }
}

void stop_capture(int fd){
    enum v4l2_buf_type type = DATASTREAM;
    // Stop streaming frames into buffer
    if(xioctl(fd, VIDIOC_STREAMOFF, &type) == -1){
        perror("Stream OFF");
        exit(errno);
    }
}


static int get_frame(int fd, int buffer_cnt){
    struct v4l2_buffer buff;
    memset(&buff, 0, sizeof(buff));
    buff.type = DATASTREAM;
    buff.memory = MEMORY;

    // dequeue a buffer to read in memory
    if(xioctl(fd, VIDIOC_DQBUF, &buff) == -1){
        switch(errno) {
            case EAGAIN:
                return 1;
                // No buffer in the outgoing queue
            case EIO:
                // fall through
            default:
                perror("Dequeue buffer");
                exit(errno);
            }
    }
    // make sure you are reading the desired frame from the requested frames in init_buffer
    assert(buff.index < buffer_cnt);

    printf("Frame %d\n",buff.index); // print which frame was captured
    

    // enqueue the buffer again
    fputc('.',stdout);
    fflush(stdout);
    if(xioctl(fd, VIDIOC_QBUF, &buff) == -1){
        perror("enqueue at read");
        exit(errno);
    }
    return 0;
}

void monitor_fd(int fd){
    fd_set fds;
    struct timeval tv;
    int r;
    FD_ZERO(&fds);
    FD_SET(fd,&fds);

    tv.tv_sec = 2;
    tv.tv_usec = 0;

    r = select(fd+1,&fds, NULL,NULL,&tv);
    if(r == -1){
        perror("Monitoring");
        exit(errno);
    }
    if(r == 0){
        fprintf(stderr, "select timeout\n");
        exit(EXIT_FAILURE);
    }
}

void save_image(int fd,Buffer *buffer, int frames, int buffer_cnt){
    char name[50];
    char *ext =  ".jpeg";
    for(int i = 0; i<frames;  i++){
        for(;;){
            monitor_fd(fd);
            if(!get_frame(fd,buffer_cnt)){
            sprintf(name, "%d", i);
            strcat(name,ext);
            int fd = open(name,O_RDWR|O_CREAT, 0666);
            write(fd,buffer[i%buffer_cnt].data, buffer[i%buffer_cnt].length);
            close(fd);
            }
            break;
        }
        //process_image(buffer,i);
    }
}

static void process_image(Buffer *buffer, int i){
    
    char filename[15];
    sprintf(filename, "frame-%d.raw", i);
    FILE *fp=fopen("lol.mp4","wb");
    fwrite(buffer[i].data, buffer[i].length, 1, fp);
    fflush(fp);
    fclose(fp);
}