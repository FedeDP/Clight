#ifndef DISABLE_FRAME_CAPTURES

#include "../inc/camera.h"

static void open_device(const char *interface);
static void init(void);
static void init_mmap(void);
static int xioctl(int request, void *arg);
static void start_stream(void);
static void stop_stream(void);
static void capture_frame(int idx);
static double compute_brightness(unsigned int length);
static double compute_backlight(void);
static void free_all();

struct state {
    int quit, width, height, device_fd, num_frames, buf_size;
    double *brightness_value;
    uint8_t *buffer;
    tjhandle _jpegDecompressor;
};

static struct state *state;

double capture_frames(const char *interface, int num_frames, int *err) {
    double val = 0.0;
    
    /* properly initialize struct with all fields to zero-or-null */
    struct state tmp = {0};
    state = malloc(sizeof(struct state));
    *state = tmp;
    
    open_device(interface);
    if (!state->quit) {
        init();
        if (state->quit) {
            goto end;
        }
        
        init_mmap();
        if (state->quit) {
            goto end;
        }
        
        state->num_frames = num_frames;
        state->brightness_value = calloc(state->num_frames, sizeof(double));
        state->_jpegDecompressor = tjInitDecompress();
        
        start_stream();
        if (state->quit) {
            goto end;
        }
        
        for (int i = 0; i < state->num_frames; i++) {
            capture_frame(i);
            if (state->quit) {
                break;
            }
        }
        
        stop_stream();
        
        if (!state->quit) {
            val = compute_backlight();
        }
    }
    
end:
    if (state->quit) {
        *err = state->quit;
    }
    free_all();
    return val;
}

static void open_device(const char *interface) {
    state->device_fd = open(interface, O_RDWR);
    if (state->device_fd == -1) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
             interface, errno, strerror(errno));
        state->quit = 1;
    }
}

static void init(void) {
    struct v4l2_capability caps = {{0}};
    if (-1 == xioctl(VIDIOC_QUERYCAP, &caps)) {
        perror("Querying Capabilities");
        return;
    }
    
    // check if it is a capture dev
    if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "No video capture device\n");
        state->quit = 1;
        return;
    }
    
    // check if it does support streaming
    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming i/o\n");
        state->quit = 1;
        return;
    }
    
    // check device priority level. Do not need to quit if this is not supported.
    enum v4l2_priority priority = V4L2_PRIORITY_BACKGROUND;
    if (-1 == xioctl(VIDIOC_S_PRIORITY, &priority)) {
        perror("Setting priority");
        state->quit = 0;
    }
    
    // query format
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    
    if (-1 == xioctl(VIDIOC_S_FMT, &fmt)) {
        perror("Setting Pixel Format");
        return;
    }
    
    state->width = fmt.fmt.pix.width;
    state->height = fmt.fmt.pix.height;
}

static void init_mmap(void) {
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    
    if (-1 == xioctl(VIDIOC_REQBUFS, &req)) {
        perror("Requesting Buffer");
        return;
    }
    
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(VIDIOC_QUERYBUF, &buf)) {
        perror("Querying Buffer");
        return;
    }
        
    state->buffer = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, state->device_fd, buf.m.offset);
    if (MAP_FAILED == state->buffer) {
        state->quit = 1;
        perror("mmap");
    }
    
    state->buf_size = buf.length;
}

static int xioctl(int request, void *arg) {
    int r;
    
    do {
        r = ioctl (state->device_fd, request, arg);
    } while (-1 == r && EINTR == errno);
    
    if (r == -1) {
        state->quit = errno;
    }
    return r;
}

static void start_stream(void) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    
    // start stream
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(VIDIOC_STREAMON, &type)) {
        perror("Start Capture");
        return;
    }
    
    // query a buffer from camera
    if(-1 == xioctl(VIDIOC_QBUF, &buf)) {
        perror("Query Buffer");
        stop_stream();
    }
}

static void stop_stream(void) {
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        
    if(-1 == xioctl(VIDIOC_STREAMOFF, &type)) {
        perror("Stop Capture");
    }
}

static void capture_frame(int idx) {
    struct v4l2_buffer buf = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    // dequeue the buffer
    if(-1 == xioctl(VIDIOC_DQBUF, &buf)) {
        perror("Retrieving Frame");
        return;
    }

    state->brightness_value[idx] = compute_brightness(buf.bytesused);
    
    // query another buffer only if we did not still reach num_frames
    if (idx < state->num_frames) {
        if (-1 == xioctl(VIDIOC_QBUF, &buf)) {
            perror("Query Buffer");
        }
    }
}

static double compute_brightness(unsigned int length) {
    double brightness = 0.0;
    size_t total_gray_pixels = state->width * state->height;
    unsigned char *blob = malloc(total_gray_pixels);
    
    if (tjDecompress2(state->_jpegDecompressor, state->buffer, length, 
                    blob, state->width, 0, state->height, 
                    TJPF_GRAY, TJFLAG_FASTDCT) == -1) {
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

static double compute_backlight(void) {
    int lowest = 0, highest = 0;
    double total = 0.0;
    
    for (int i = 0; i < state->num_frames; i++) {
        if (state->brightness_value[i] < state->brightness_value[lowest]) {
            lowest = i;
        } else if (state->brightness_value[i] > state->brightness_value[highest]) {
            highest = i;
        }
        
        total += state->brightness_value[i];
    }
        
    // total == 0.0 means every captured frame decompression failed
    if (total != 0.0 && state->num_frames > 2) {
        // remove highest and lowest values to normalize
        total -= (state->brightness_value[highest] + state->brightness_value[lowest]);
        return total / (state->num_frames - 2);
    }
        
    return total / state->num_frames;
}

static void free_all(void) {
    if (state->device_fd != -1) {
        close(state->device_fd);
    }
    
    if (state->brightness_value) {
        free(state->brightness_value);
    }
    
    if (state->buffer) {
        munmap(state->buffer, state->buf_size);
    }
    
    if (state->_jpegDecompressor) {
        tjDestroy(state->_jpegDecompressor);
    }
    
    free(state);
}

#endif
