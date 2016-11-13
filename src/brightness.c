#include "../inc/brightness.h"

struct brightness {
    int current;
    int max;
    int old;
};

static struct brightness br;
static MagickWand *wand;

void init_brightness(void) {
    MagickWandGenesis();
    
    wand = NewMagickWand();
    
    FILE *f = fopen("/sys/class/backlight/intel_backlight/max_brightness", "r");
    if (!f) {
        goto error;
    }
    fscanf(f, "%d", &br.max);
    fclose(f);
    
    _log(stdout, "Max supported brightness: %d\n", br.max);
    
    f = fopen("/sys/class/backlight/intel_backlight/brightness", "r");
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

float compute_brightness(unsigned int length) {
    MagickReadImageBlob(wand, buffer, length);
    size_t total_gray_pixels = camera_width * camera_height;
    
    // Allocate memory to hold values (total pixels * size of data type)
    unsigned char *blob = malloc(total_gray_pixels);
    
    MagickExportImagePixels(wand,      // Image instance
                            0,         // Start X
                            0,         // Start Y
                            camera_width,     // End X
                            camera_height,    // End Y
                            "I",       // Value where "I" = intensity = gray value
                            CharPixel, // Storage type where "unsigned char == (0 ~ 255)
                            blob);     // Destination pointer
    
    // Dump to stdout
    unsigned long brightness = 0;
    for (int i = 0; i < total_gray_pixels; i++ ) {
        brightness += (int)blob[i];
    }
    brightness /= total_gray_pixels;
    
    free(blob);
    return (float)(brightness) / 255;
}

float set_brightness(float perc) {
    br.old = br.current;
    
    FILE *f = fopen("/sys/class/backlight/intel_backlight/brightness", "w");
    if (f) {
        fprintf(f, "%d", (int) (br.max * perc));
        fclose(f);
    } else {
        _perror("Not authorized");
    }
    
    br.current = (int) (br.max * perc);
    
    _log(stdout, "New brightness value: %d\nOld brightness value: %d\n", br.current, br.old);
    
    return (float)(br.current - br.old) / br.max;
}

void free_wand(void) {
    if (wand) {
        wand = DestroyMagickWand(wand);
        MagickWandTerminus();
    }
}
