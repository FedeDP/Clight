#ifndef DISABLE_FRAME_CAPTURES

#include "../inc/camera.h"

static int open_device(const char *interface, int *quit);
static void init(int fd, int *quit, int *width, int *height);
static int init_mmap(int fd, uint8_t **buffer, int *quit);
static int xioctl(int fd, int request, void *arg);
static void start_stream(int fd, int *quit);
static void stop_stream(int fd, int *quit);
static void capture_frame(int idx, double *brightness_value, int num_frames, uint8_t *buffer, tjhandle _jpegDecompressor, int fd, int *quit, int camera_width, int camera_height);
static double compute_brightness(unsigned int length, uint8_t *buffer, tjhandle _jpegDecompressor, int camera_width, int camera_height);
static double compute_backlight(int num_frames, double *brightness_value);
static void free_all(int fd, double *ptr, int buf_size, uint8_t *buffer, tjhandle _jpegDecompressor);

double capture_frames(const char *interface, int num_frames) {
    double *brightness_value = NULL;
    int buf_size, quit = 0, width, height;
    uint8_t *buffer = NULL;
    tjhandle _jpegDecompressor = NULL;
    double val = -1;
    
    int device_fd = open_device(interface, &quit);
    if (!quit) {
        
        init(device_fd, &quit, &width, &height);
        if (quit) {
            goto end;
        }
        
        buf_size = init_mmap(device_fd, &buffer, &quit);
        if (quit) {
            goto end;
        }
        
        brightness_value = calloc(num_frames, sizeof(double));
        _jpegDecompressor = tjInitDecompress();
        
        start_stream(device_fd, &quit);
        if (quit) {
            goto end;
        }
        
        for (int i = 0; i < num_frames; i++) {
            capture_frame(i, brightness_value, num_frames, buffer, _jpegDecompressor, device_fd, &quit, width, height);
            if (quit) {
                break;
            }
        }
        
        stop_stream(device_fd, &quit);
        
        if (!quit) {
            val = compute_backlight(num_frames, brightness_value);
        }
    }
    
end:
    free_all(device_fd, brightness_value, buf_size, buffer, _jpegDecompressor);
    return val;
}

static int open_device(const char *interface, int *quit) {
    int device_fd = open(interface, O_RDWR);
    if (device_fd == -1) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
             interface, errno, strerror(errno));
        *quit = 1;
    }
    return device_fd;
}

static void init(int fd, int *quit, int *width, int *height) {
    struct v4l2_capability caps = {{0}};
    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps)) {
        perror("Querying Capabilities");
        goto error;
    }
    
    // check if it is a capture dev
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "No video capture device\n");
        goto error;
    }
    
    // check if it does support streaming
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming i/o\n");
        goto error;
    }
    
    // check device priority level. Do not need to quit if this is not supported.
    enum v4l2_priority priority = V4L2_PRIORITY_BACKGROUND;
    if (-1 == xioctl(fd, VIDIOC_S_PRIORITY, &priority)) {
        perror("Setting priority");
    }
    
    // query format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) {
        perror("Setting Pixel Format");
        goto error;
    }
    
    *width = fmt.fmt.pix.width;
    *height = fmt.fmt.pix.height;
    
    return;
    
error:
    *quit = 1;
}

static int init_mmap(int fd, uint8_t **buffer, int *quit) {
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        perror("Requesting Buffer");
        goto error;
    }
    
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
        perror("Querying Buffer");
        goto error;
    }
        
    *buffer = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    
    if (MAP_FAILED == *buffer) {
        perror("mmap");
        goto error;
    }
    return buf.length;
    
error:
    *quit = 1;
    return -1;
}

static int xioctl(int fd, int request, void *arg) {
    int r;
    
    do {
        r = ioctl (fd, request, arg);
    } while (-1 == r && EINTR == errno);
    
    return r;
}

static void start_stream(int fd, int *quit) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    
    // start stream
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
        perror("Start Capture");
        *quit = 1;
        return;
    }
    
    // query a buffer from camera on device_fd so main poll will read from CAMERA_IX
    if(-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
        perror("Query Buffer");
        *quit = 1;
        stop_stream(fd, quit);
    }
}

static void stop_stream(int fd, int *quit) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        
    if(-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)) {
        perror("Stop Capture");
        *quit = 1;
    }
}

static void capture_frame(int idx, double *brightness_value, int num_frames, 
                          uint8_t *buffer, tjhandle _jpegDecompressor, int fd, 
                          int *quit, int camera_width, int camera_height) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    // dequeue the buffer
    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        perror("Retrieving Frame");
        *quit = 1;
        return;
    }

    brightness_value[idx] = compute_brightness(buf.bytesused, buffer, _jpegDecompressor, camera_width, camera_height);
    
    // query another buffer only if we did not still reach num_frames
    if (idx < num_frames) {
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            perror("Query Buffer");
            *quit = 1;
        }
    }
}

static double compute_brightness(unsigned int length, uint8_t *buffer, tjhandle _jpegDecompressor, int camera_width, int camera_height) {
    double brightness = 0.0;
    size_t total_gray_pixels = camera_width * camera_height;
    unsigned char *blob = malloc(total_gray_pixels);
    
    if (tjDecompress2(_jpegDecompressor, buffer, length, blob, camera_width, 0, camera_height, TJPF_GRAY, TJFLAG_FASTDCT) == -1) {
        fprintf(stderr, "%s", tjGetErrorStr());
        goto end;
    }
    
    for (int i = 0; i < total_gray_pixels; i++ ) {
        brightness += (int)blob[i];
    }
    brightness /= total_gray_pixels;
    
end:
    free(blob);
    return brightness / 255;
}

static double compute_backlight(int num_frames, double *brightness_value) {
    int lowest = 0, highest = 0;
    double total = 0.0;
    
    for (int i = 0; i < num_frames; i++) {
        if (brightness_value[i] < brightness_value[lowest]) {
            lowest = i;
        } else if (brightness_value[i] > brightness_value[highest]) {
            highest = i;
        }
        
        total += brightness_value[i];
    }
        
    // total == 0.0 means every captured frame decompression failed
    if (total != 0.0 && num_frames > 2) {
        // remove highest and lowest values to normalize
        total -= (brightness_value[highest] + brightness_value[lowest]);
    }
        
    return (num_frames > 2) ? (total / (num_frames - 2)) : (total / num_frames);
}

static void free_all(int fd, double *ptr, int buf_size, uint8_t *buffer, tjhandle _jpegDecompressor) {
    if (fd != -1) {
        close(fd);
    }
    
    if (ptr) {
        free(ptr);
    }
    
    if (buffer) {
        munmap(buffer, buf_size);
    }
    
    if (_jpegDecompressor) {
        tjDestroy(_jpegDecompressor);
    }
}
#endif
