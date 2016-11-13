#include "../inc/camera.h"

static int init_mmap(void);
static int xioctl(int request, void *arg);
static int print_caps(void);

static int device_fd, captured_frames;
static const char dev_name[] = "/dev/video0";
static float *brightness_value;
static struct v4l2_buffer buf;

void open_device(void) {
    device_fd = open(dev_name, O_RDWR);
    if (device_fd == -1) {
        _log(stderr, "Cannot open '%s': %d, %s\n",
             dev_name, errno, strerror(errno));
        quit = 1;
    } else {
        brightness_value = malloc(num_captures * sizeof(float));
        print_caps();
        init_mmap();
    }
}

static int print_caps(void) {
    struct v4l2_capability caps = {0};
    if (-1 == xioctl(VIDIOC_QUERYCAP, &caps)) {
        _perror("Querying Capabilities");
        return 1;
    }
    
    _log(stdout, "Driver Caps:\n"
    "  Driver: \"%s\"\n"
    "  Card: \"%s\"\n"
    "  Bus: \"%s\"\n"
    "  Version: %d.%d\n"
    "  Capabilities: %08x\n",
    caps.driver,
    caps.card,
    caps.bus_info,
    (caps.version>>16)&&0xff,
         (caps.version>>24)&&0xff,
         caps.capabilities);
    
    
    struct v4l2_cropcap cropcap = {0};
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(VIDIOC_CROPCAP, &cropcap)) {
        _perror("Querying Cropping Capabilities");
        return 1;
    }
    
    _log(stdout, "Camera Cropping:\n"
    "  Bounds: %dx%d+%d+%d\n"
    "  Default: %dx%d+%d+%d\n"
    "  Aspect: %d/%d\n",
    cropcap.bounds.width, cropcap.bounds.height, cropcap.bounds.left, cropcap.bounds.top,
    cropcap.defrect.width, cropcap.defrect.height, cropcap.defrect.left, cropcap.defrect.top,
    cropcap.pixelaspect.numerator, cropcap.pixelaspect.denominator);
    
    struct v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    char fourcc[5] = {0};
    char c, e;
    _log(stdout, "  FMT : CE Desc\n--------------------\n");
    while (0 == xioctl(VIDIOC_ENUM_FMT, &fmtdesc)) {
        strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
        c = fmtdesc.flags & 1? 'C' : ' ';
        e = fmtdesc.flags & 2? 'E' : ' ';
        _log(stdout, "  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
        fmtdesc.index++;
    }
    
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (-1 == xioctl(VIDIOC_S_FMT, &fmt)) {
        _perror("Setting Pixel Format");
        return 1;
    }
    
    strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
    _log(stdout, "Selected Camera Mode:\n"
    "  Width: %d\n"
    "  Height: %d\n"
    "  PixFmt: %s\n"
    "  Field: %d\n",
    fmt.fmt.pix.width,
    fmt.fmt.pix.height,
    fourcc,
    fmt.fmt.pix.field);
    
    camera_width = fmt.fmt.pix.width;
    camera_height = fmt.fmt.pix.height;
    return 0;
}

static int init_mmap(void) {
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(VIDIOC_REQBUFS, &req)) {
        _perror("Requesting Buffer");
        return 1;
    }
    
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(VIDIOC_QUERYBUF, &buf)) {
        _perror("Querying Buffer");
        return 1;
    }
    
    buffer = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, device_fd, buf.m.offset);
    return 0;
}

static int xioctl(int request, void *arg) {
    int r;
    
    do {
        r = ioctl (device_fd, request, arg);
    } while (-1 == r && EINTR == errno);
    
    return r;
}

int start_stream(void) {
    memset(&buf, 0, sizeof(buf));
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    
    // query a buffer from camera on device_fd
    if(-1 == xioctl(VIDIOC_QBUF, &buf)) {
        _perror("Query Buffer");
        return -1;
    }
    
    // start stream
    if(-1 == xioctl(VIDIOC_STREAMON, &buf.type)) {
        _perror("Start Capture");
        return -1;
    }
    
    return device_fd;
}

int stop_stream(void) {
    captured_frames = 0;
    
    if(-1 == xioctl(VIDIOC_STREAMOFF, &buf.type)) {
        _perror("Stop Capture");
        return -1;
    }
    return 0;
}

int capture_frame(void) {
    // dequeue the buffer
    if(-1 == xioctl(VIDIOC_DQBUF, &buf)) {
        _perror("Retrieving Frame");
        return -1;
    }

    brightness_value[captured_frames] = compute_brightness(buf.bytesused);
    _log(stdout, "Image %d brightness: %f\n", captured_frames + 1, brightness_value[captured_frames]);
    captured_frames++;
    
    // query another buffer
    if(-1 == xioctl(VIDIOC_QBUF, &buf)) {
        _perror("Query Buffer");
        return -1;
    }
    
    return captured_frames;

}

float compute_backlight(void) {
    int lowest = 0, highest = 0;
    float total = 0;
    
    for (int i = 1; i < num_captures; i++) {
        if (brightness_value[i] < brightness_value[lowest]) {
            lowest = i;
        } else if (brightness_value[i] > brightness_value[highest]) {
            highest = i;
        }
        total += brightness_value[i];
    }
    
    // remove highest and lowest values to normalize
    total -= (brightness_value[highest] + brightness_value[lowest]); 
    
    return total / (num_captures - 2);
}

void free_device(void) {
    close(device_fd);
    if (brightness_value) {
        free(brightness_value);
    }
}
