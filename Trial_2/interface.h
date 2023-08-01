#include <linux/videodev2.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
/*  Options:
            V4L2_BUF_TYPE_VIDEO_CAPTURE
            V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
            V4L2_BUF_TYPE_VIDEO_OUTPUT
            V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
            V4L2_BUF_TYPE_VIDEO_OVERLAY
*/
#define DATASTREAM          V4L2_BUF_TYPE_VIDEO_CAPTURE
#define DEVICE              "/dev/video2"
/*  Options:
            V4L2_PIX_FMT_YUYV
            V4L2_PIX_FMT_JPEG
*/
#define PIXEL_FMT           V4L2_PIX_FMT_MJPEG
#define BUFFER_FRAMES       20
#define MIN_BUFFER_FRAMES   5


// structure of buffer 
/*
    data:   Holds starting address of the frame
    length: Holds the length of the frame in the buffer segment
*/
typedef struct buf{
    void *data;
    size_t length;
}Buffer;

//  ioctl wrapper 
//  keeps the sys call within until it returns WHEN the system call is interrupted internally suddenly (unlikely to happen but it can occur)
static int xioctl(int fd, int request, void *arg);
/*
    fd: file descriptor (device)
    request: ioctl sys call request
    arg: (optional) writes or modifies argument, usually a v4l2 struct
*/

//  prints description for the device capabilties 
//  returns pixelformate
void device_desc(int fd);
/*  types of descriptions:
                        1) Device capabilities
                        2) Device crop capabilties
                        3) supported formates
*/  

//  sets picture formate and dimnsion
//  First supported format is chosen by default if an invalid format was inserted
//  Formates is predefined in PIXEL_FMT macro
void set_formate(int fd, int width, int height, int pixel_formate);

//  initiate buffer aquistion of data
//  first step is to request from the device buffer to be working with memory mapping technique
//  after request is done ioctl is done to get device buffer in buff argument and mmap it with the user space buffer
int init_buffer(int fd, Buffer *buffer);
// returns buffer counts