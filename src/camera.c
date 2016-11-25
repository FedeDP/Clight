#include "../inc/camera.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static void init_mmap(void);
static int xioctl(int request, void *arg);
static void init(void);

static int device_fd, captured_frames;
static unsigned int buf_size;
static double *brightness_value;
static struct v4l2_buffer buf;

void open_device(void) {
    device_fd = open(conf.dev_name, O_RDWR);
    if (device_fd == -1) {
        _log(stderr, "Cannot open '%s': %d, %s\n",
             conf.dev_name, errno, strerror(errno));
        quit = 1;
    } else {
        brightness_value = malloc(conf.num_captures * sizeof(double));
        init();
    }
}

static void init(void) {
    struct v4l2_capability caps = {{0}};
    if (-1 == xioctl(VIDIOC_QUERYCAP, &caps)) {
        _perror("Querying Capabilities");
        goto error;
    }
    
    // check if it is a capture dev
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        _log(stderr, "%s is no video capture device\n",
                conf.dev_name);
        goto error;
    }
    
    // check if it does support streaming
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        _log(stderr, "%s does not support streaming i/o\n",
                conf.dev_name);
        goto error;
    }
    
    // check device priority level. Do not need to quit if this is not supported.
    enum v4l2_priority priority = V4L2_PRIORITY_BACKGROUND;
    if (-1 == xioctl(VIDIOC_S_PRIORITY, &priority)) {
        _perror("Setting priority");
    }
    
    // query format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (-1 == xioctl(VIDIOC_S_FMT, &fmt)) {
        _perror("Setting Pixel Format");
        goto error;
    }
    
    char fourcc[5] = {0};
    strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
    _log(stdout, "Selected Camera Mode:\n"
    "  Width: %d\n"
    "  Height: %d\n"
    "  PixFmt: %s\n",
    fmt.fmt.pix.width,
    fmt.fmt.pix.height,
    fourcc);
    
    camera_width = fmt.fmt.pix.width;
    camera_height = fmt.fmt.pix.height;
    
    return init_mmap();
    
error:
    quit = 1;
}

static void init_mmap(void) {
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(VIDIOC_REQBUFS, &req)) {
        _perror("Requesting Buffer");
        goto error;
    }
    
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(VIDIOC_QUERYBUF, &buf)) {
        _perror("Querying Buffer");
        goto error;
    }
    
    buf_size = buf.length;
    
    buffer = mmap (NULL, buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, device_fd, buf.m.offset);
    
    if (MAP_FAILED == buffer) {
        _perror("mmap");
        goto error;
    }
    return;
    
error:
    quit = 1;
}

static int xioctl(int request, void *arg) {
    int r;
    
    do {
        r = ioctl (device_fd, request, arg);
    } while (-1 == r && EINTR == errno);
    
    return r;
}

int start_stream(void) {
    CLEAR(buf);
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    
    // start stream
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(VIDIOC_STREAMON, &type)) {
        _perror("Start Capture");
        return -1;
    }
    
    // query a buffer from camera on device_fd so main poll will read from CAMERA_IX
    if(-1 == xioctl(VIDIOC_QBUF, &buf)) {
        _perror("Query Buffer");
        return -1;
    }
    
    return device_fd;
}

int stop_stream(void) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    captured_frames = 0;
    
    if(-1 == xioctl(VIDIOC_STREAMOFF, &type)) {
        _perror("Stop Capture");
        return -1;
    }
    return 0;
}

int capture_frame(void) {
    CLEAR(buf);
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    // dequeue the buffer
    if(-1 == xioctl(VIDIOC_DQBUF, &buf)) {
        _perror("Retrieving Frame");
        return -1;
    }

    brightness_value[captured_frames] = compute_brightness(buf.bytesused);
    _log(stdout, "Image %d brightness: %f\n", captured_frames + 1, brightness_value[captured_frames]);
    captured_frames++;
    
    // query another buffer only if we did not still reach conf.num_captures
    if (captured_frames < conf.num_captures) {
        if (-1 == xioctl(VIDIOC_QBUF, &buf)) {
            _perror("Query Buffer");
            return -1;
        }
    }
    
    return captured_frames;
}

double compute_backlight(void) {
    int lowest = 0, highest = 0;
    double total = 0;
    
    for (int i = 0; i < conf.num_captures; i++) {
        if (brightness_value[i] < brightness_value[lowest]) {
            lowest = i;
        } else if (brightness_value[i] > brightness_value[highest]) {
            highest = i;
        }
        
        // compute_brightness returns -1 if it fails
        if (brightness_value[i] != -1) {
            total += brightness_value[i];
        }
    }
        
    // total == 0 means every captured frame decompression failed
    if (total != 0 && conf.num_captures > 2) {
        // remove highest and lowest values to normalize
        total -= (brightness_value[highest] + brightness_value[lowest]); 
    }
        
    return (conf.num_captures > 2) ? (total / (conf.num_captures - 2)) : (total / conf.num_captures);
}

void free_device(void) {
    close(device_fd);
    if (brightness_value) {
        free(brightness_value);
    }
    
    munmap(buffer, buf_size);
}
