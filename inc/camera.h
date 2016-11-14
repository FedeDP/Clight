#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "brightness.h"

void open_device(void);
int start_stream(void);
int stop_stream(void);
int capture_frame(void);
double compute_backlight(void);
void free_device(void);
