#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <unistd.h>
#include <string.h>

#ifndef DISABLE_FRAME_CAPTURES

#include <sys/mman.h>
#include <linux/videodev2.h>
#include <turbojpeg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>


double capture_frames(const char *interface, int num_frames);

#endif
