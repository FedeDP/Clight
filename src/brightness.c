#include "../inc/brightness.h"

struct brightness {
    int current;
    int max;
    int old;
};

static tjhandle _jpegDecompressor;
static struct brightness br;
char brightness_path[PATH_MAX + 1];

void init_brightness(void) {
    _jpegDecompressor = tjInitDecompress();
    
    char max_brightness_path[PATH_MAX + 1];
    
    sprintf(max_brightness_path, "%s/max_brightness", conf.screen_path);
    
    FILE *f = fopen(max_brightness_path, "r");
    if (!f) {
        goto error;
    }
    fscanf(f, "%d", &br.max);
    fclose(f);
    
    _log(stdout, "Max supported brightness: %d\n", br.max);
    
    sprintf(brightness_path, "%s/brightness", conf.screen_path);
    
    f = fopen(brightness_path, "r");
    if (!f) {
        goto error;
    }
    fscanf(f, "%d", &br.current);
    fclose(f);
    
    _log(stdout, "Current brightness: %d\n", br.max);
    
    return;
    
error:
    quit = 1;
}

double compute_brightness(unsigned int length) {
    double brightness = 0;
    size_t total_gray_pixels = camera_width * camera_height;
    unsigned char *blob = malloc(total_gray_pixels);
    
    if (tjDecompress2(_jpegDecompressor, buffer, length, blob, camera_width, 0, camera_height, TJPF_GRAY, TJFLAG_FASTDCT) == -1) {
        _log(stderr, "%s", tjGetErrorStr());
        // returns -1 (-255 / 255) as error code
        brightness = -255;
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

double set_brightness(double perc) {
    br.old = br.current;
    
    FILE *f = fopen(brightness_path, "w");
    if (f) {
        fprintf(f, "%d", (int) (br.max * perc));
        fclose(f);
    } else {
        _perror("Not authorized");
    }
    
    br.current = (int) (br.max * perc);
    
    _log(stdout, "New brightness value: %d\nOld brightness value: %d\n", br.current, br.old);
    
    return (double)(br.current - br.old) / br.max;
}

void free_brightness(void) {
    tjDestroy(_jpegDecompressor);
}
