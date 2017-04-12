/**
 * Thanks to http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/ 
 * and to improvements made here: http://www.zombieprototypes.com/?p=210.
 **/

#ifndef DISABLE_GAMMA

#include "../inc/gamma.h"
#include <X11/extensions/Xrandr.h>
#include <math.h>

static double clamp(double x, double max);
static double get_red(int temp);
static double get_green(int temp);
static double get_blue(int temp);
static double get_temp(const unsigned short R, const unsigned short B);

static double clamp(double x, double max) {
    if (x > max) { 
        return max; 
    }
    return x;
}

static double get_red(int temp) {
    if (temp <= 6500) {
        return 255;
    }
    const double a = 351.97690566805693;
    const double b = 0.114206453784165;
    const double c = -40.25366309332127;
    const double new_temp = ((double)temp / 100) - 55;
    
    return clamp(a + b * new_temp + c * log(new_temp), 255);
}

static double get_green(int temp) {
    double a, b, c;
    double new_temp;
    if (temp <= 6500) {
        a = -155.25485562709179;
        b = -0.44596950469579133;
        c = 104.49216199393888;
        new_temp = ((double)temp / 100) - 2;
    } else {
        a = 325.4494125711974;
        b = 0.07943456536662342;
        c = -28.0852963507957;
        new_temp = ((double)temp / 100) - 50;        
    }
    return clamp(a + b * new_temp + c * log(new_temp), 255);
}

static double get_blue(int temp) {
    if (temp <= 1900) {
        return 0;
    }
    
    if (temp < 6500) {
        const double new_temp = ((double)temp / 100) - 10;
        const double a = -254.76935184120902;
        const double b = 0.8274096064007395;
        const double c = 115.67994401066147;
        
        return clamp(a + b * new_temp + c * log(new_temp), 255);
    }
    return 255;
}

/* Thanks to: https://github.com/neilbartlett/color-temperature/blob/master/index.js */
static double get_temp(const unsigned short R, const unsigned short B) {
    const double epsilon=0.4;
    double temperature;
    double min_temp = B == 255 ? 6500 : 1000; // lower bound
    double max_temp = R == 255 ? 6500 : 10000; // upper bound
    do {
        temperature = (max_temp + min_temp) / 2;
        unsigned short testR = get_red(temperature);
        unsigned short testB = get_blue(temperature);
        if (testR == R && testB == B) {
            break;
        }
        if ((double) testB / testR > (double) B / R) {
            max_temp = temperature;
        } else {
            min_temp = temperature;
        }
    } while (max_temp - min_temp > epsilon);
    return round(temperature);
}

void set_gamma(int temp, int *err) {
    Display *dpy = XOpenDisplay(NULL);

    if (dpy == NULL) {
        *err = ENXIO;
        return;
    }

    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    
    double red = get_red(temp) / 255;
    double green = get_green(temp) / 255;
    double blue = get_blue(temp) / 255;
        
    for (int i = 0; i < res->ncrtc; i++) {
        int crtcxid = res->crtcs[i];

        int size = XRRGetCrtcGammaSize(dpy, crtcxid);

        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);

        for (int j = 0; j < size; j++) {
            double g = 65535.0 * j / size;
            crtc_gamma->red[j] = g * red;
            crtc_gamma->green[j] = g * green;
            crtc_gamma->blue[j] = g * blue;
        }
        XRRSetCrtcGamma(dpy, crtcxid, crtc_gamma);
        XFree(crtc_gamma);
    }

    XCloseDisplay(dpy);
}

int get_gamma(int *err) {
    int temp = -1;
    Display *dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        *err = ENXIO;
        goto end;
    }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);

    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    if (res->ncrtc > 0) {
        XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma(dpy, res->crtcs[0]);
        temp = get_temp(crtc_gamma->red[1], crtc_gamma->blue[1]);
        XFree(crtc_gamma);
    }

    if (temp <= 0) {
        *err = 1;
    }

    XCloseDisplay(dpy);

end:
    return temp;
}

#endif
