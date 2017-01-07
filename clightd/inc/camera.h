#ifndef DISABLE_FRAME_CAPTURES

#include <unistd.h>
#include <string.h>

#include <sys/mman.h>
#include <linux/videodev2.h>
#include <turbojpeg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include "commons.h"

double capture_frames(const char *interface, int *err);

#endif
