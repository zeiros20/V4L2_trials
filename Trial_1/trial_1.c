/*
        ioctl is system call used to interface with some device and communicate with it
        Mainly ioctl is used to manipulate device parameters 
        In ioctl system call there will be 3 main parameters 

        fd: represents the file descripter (in this case open() is used with device file /dev/video0 which is the default laptop cam)
        request: it's the type of request needed from the ioctl sys call 
            options are many at each function written i will write which request done and what does it do
        arg: is optional parameter usually has a predefined structure of the v4l2 lib 
            b4 using a structure it's initialized with {0} and then call it by refrence in ioctl

        ioctl returns 0 upon success 
        upon return of -1 errno is set to identify errors
            errno:
                EBADF: fd is not vaild
                EFAULT: arg is inaccessible memory area
                EINVAL: requenst or arg are invalid
                ENOTTY: fd is associated with special device || request can't be done for the current fd ?? (man page seems to be joking FUCK IT)

*/

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


#define DATASTREAM V4L2_BUF_TYPE_VIDEO_CAPTURE


struct {
    void *start;
    size_t length;
} *buffer;


// VIDIOC_QUERYCAP a request upon pass it initialize the v4l2_capability struce with the opened device capabilities
// in print_cap as the name says I will print a device caps

int print_cap(int fd){
    int ret;
    struct v4l2_capability caps = {0}; // init the v4l2_capability with 0
    ret = ioctl(fd, VIDIOC_QUERYCAP, &caps);
    if (ret == -1) {
        perror("Querying device capabilities");
        return errno;
    }
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
    
    return 0;

}

// VIDIOC_CROPCAP is just like VIDIOC_QUERYCAP but for crop capabilities of device
// a thing to note is the cropcaps.type which is the datastream type 
/*
        options are:
            V4L2_BUF_TYPE_VIDEO_CAPTURE
            V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
            V4L2_BUF_TYPE_VIDEO_OUTPUT
            V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
            V4L2_BUF_TYPE_VIDEO_OVERLAY
*/
int print_crop_cap(int fd){
    int ret;
    struct v4l2_cropcap cropcaps = {0};
    cropcaps.type = DATASTREAM;
    ret = ioctl(fd, VIDIOC_CROPCAP, &cropcaps);
    if (ret == -1) {
      perror("Querying device crop capabilities");
      return errno;
    }

    printf("Camera Cropping:\n"
           "  Bounds: %dx%d+%d+%d\n"
           "  Default: %dx%d+%d+%d\n"
           "  Aspect: %d/%d\n",
           cropcaps.bounds.width, cropcaps.bounds.height, cropcaps.bounds.left, cropcaps.bounds.top,
           cropcaps.defrect.width, cropcaps.defrect.height,
    	 cropcaps.defrect.left, cropcaps.defrect.top,
           cropcaps.pixelaspect.numerator, cropcaps.pixelaspect.denominator);  
    return 0;
}

//VIDIOC_ENUM_FMT request is used to enumrate all supported image formates for a given device 
//Once again datastream is specified in the v4l2_fmtdesc.type

unsigned int print_supp_formates(int fd){
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = DATASTREAM;
    char c, e;
    /*
        Formates can be either compressed or emulated each is held in the fmtdesc.flags 
        c & e char will used to hold each by using the ternary operation ? to see which is which
        fmtdesc is enumrated so indexing is essintial and whenever fmtdesc.index exceed the actual size of indexes ioctl will return -1
    */
    while(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
      c = fmtdesc.flags & 1 ? 'C' : ' ';
      e = fmtdesc.flags & 2 ? 'E' : ' ';

      printf("%c%c %s\n", c, e, fmtdesc.description);
      fmtdesc.index++;
    }

     printf("\nUsing format: %s\n", fmtdesc.description);

     return fmtdesc.pixelformat;

}

// VIDIOC_S_FMT request sets the formate in a way ioctl with such request communicate with the device hardware to ensure the desired formate
// V4L2_formate struct carries the formate options for the desired formate they are set by calling function parameters that were set by terminal's argv
int set_foramte(int fd, int width, int height, int pixelformate){
    struct v4l2_format fmt = {0};
    fmt.type = DATASTREAM;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelformate;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (-1 == ioctl(fd, VIDIOC_S_FMT, &fmt)) {
      perror("Could not set format description");
      return -1;
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
    return 0;
}
// Streaming is an I/O method where only pointers to buffers are exchanged between application and driver, the data itself is not copied. Memory mapping is primarily intended to map buffers in device memory into the application’s address space. Device memory can be for example the video memory on a graphics card with a video capture add-on.
// First thing first we must assign to the reqbuf mmap constant which indicates that buffers will exchange data via memory mapping manner
// V4L2_MEMORY_MMAP indicates memory mapping for the reqbuf struct
// V4L2_REQBUFS request, requests buffers exchange from the device buffer
// not all the buffer size can be aquired so for the very least we will ensure there are at least 5 buffers

int request_buffer(int fd){
    struct v4l2_requestbuffers reqbuf;
    unsigned int i;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.type = DATASTREAM;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 20;

    if(ioctl(fd, VIDIOC_REQBUFS, &reqbuf) == -1){
        if(errno == EINVAL)
            printf("Video capturing or mmap is not supported");
        else
            perror("VIDIOC_REQBUFS");
        exit(EXIT_FAILURE);
    }

    if(reqbuf.count < 5){
        printf("Not enough buffer memory");
        exit(EXIT_FAILURE);
    }
    // dynamically allocate memory to make a location to store the structs that contains information
    buffer = calloc(reqbuf.count, sizeof(*buffer));
    assert(buffer != NULL);

    // starting from here we will make a userspace for the requested buffers from before
    // buff will hold the information from the requested buffers each for every available buffer
    struct v4l2_buffer buff;
    for(i=0;i< reqbuf.count;i++){
        memset(&buff, 0, sizeof(buff));
        buff.type = reqbuf.type;
        buff.memory = V4L2_MEMORY_MMAP;
        buff.index = i;
        // request information withing the buffer
        // that is done with ioctl request VIDOC_QUERYBUF
        if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buff)) {
            perror("VIDIOC_QUERYBUF");
            exit(EXIT_FAILURE);
        }
        buffer[i].length = buff.length;
        buffer[i].start = mmap(NULL,buff.length, PROT_READ | PROT_WRITE, MAP_SHARED,fd,buff.m.offset);
        if (MAP_FAILED == buffer[i].start) {
            perror("mmap");
            exit(EXIT_FAILURE);
        }
    }
    // Cleanup
    for (i = 0; i < reqbuf.count; i++)
        munmap(buffer[i].start, buffer[i].length);
    free(buffer);

    return 0;
}


int main(int argc, char *argv[]){
    long width = strtol(argv[2], NULL,10) , height = strtol(argv[3], NULL, 10);
    const char*  DEVICE = argv[1];
    int fd = open(DEVICE, O_RDWR);
    if(fd < 0){
        perror(DEVICE);
        return errno;
    }
    print_cap(fd);
    print_crop_cap(fd);
    unsigned int pixform =  print_supp_formates(fd);
    set_foramte(fd,(int)width,height, pixform);
    request_buffer(fd);
    close(fd);
    return 0;
}