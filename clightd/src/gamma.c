#ifndef DISABLE_GAMMA

#include "../inc/gamma.h"
#include <stdio.h>

/* cribbed from redshift, but truncated with 500K steps */
static const struct { float r; float g; float b; } whitepoints[] = {
	{ 1.00000000,  0.18172716,  0.00000000, }, /* 1000K */
	{ 1.00000000,  0.42322816,  0.00000000, },
	{ 1.00000000,  0.54360078,  0.08679949, },
	{ 1.00000000,  0.64373109,  0.28819679, },
	{ 1.00000000,  0.71976951,  0.42860152, },
	{ 1.00000000,  0.77987699,  0.54642268, },
	{ 1.00000000,  0.82854786,  0.64816570, },
	{ 1.00000000,  0.86860704,  0.73688797, },
	{ 1.00000000,  0.90198230,  0.81465502, },
	{ 1.00000000,  0.93853986,  0.88130458, },
	{ 1.00000000,  0.97107439,  0.94305985, },
	{ 1.00000000,  1.00000000,  1.00000000, }, /* 6500K */
	{ 0.95160805,  0.96983355,  1.00000000, },
	{ 0.91194747,  0.94470005,  1.00000000, },
	{ 0.87906581,  0.92357340,  1.00000000, },
	{ 0.85139976,  0.90559011,  1.00000000, },
	{ 0.82782969,  0.89011714,  1.00000000, },
	{ 0.80753191,  0.87667891,  1.00000000, },
	{ 0.78988728,  0.86491137,  1.00000000, }, /* 10000K */
	{ 0.77442176,  0.85453121,  1.00000000, },
};

int set_gamma(int temp) {
    Display *dpy = XOpenDisplay(NULL);
    
    if (dpy == NULL) {
        return -ENXIO;
    }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    
    if (temp < 1000 || temp > 10000) {
        return -EINVAL;
    }
    
    temp -= 1000;
    double ratio = temp % 500 / 500.0;
    
#define AVG(c) whitepoints[temp / 500].c * (1 - ratio) + whitepoints[temp / 500 + 1].c * ratio

    double gammar = AVG(r);
    double gammag = AVG(g);
    double gammab = AVG(b);
    
    for (int i = 0; i < res->ncrtc; i++) {
        int crtcxid = res->crtcs[i];
        
        int size = XRRGetCrtcGammaSize(dpy, crtcxid);
        
        XRRCrtcGamma *crtc_gamma = XRRAllocGamma(size);
        
        for (int j = 0; j < size; j++) {
            double g = 65535.0 * j / size;
            crtc_gamma->red[j] = g * gammar;
            crtc_gamma->green[j] = g * gammag;
            crtc_gamma->blue[j] = g * gammab;
        }
        XRRSetCrtcGamma(dpy, crtcxid, crtc_gamma);
        
        XFree(crtc_gamma);
    }
    
    XCloseDisplay(dpy);
    return 0;
}

int get_gamma(void) {
    int temp = -1;
    Display *dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        return -ENXIO;
    }
    int screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    
    if (res->ncrtc > 0) {
        XRRCrtcGamma *crtc_gamma = XRRGetCrtcGamma(dpy, res->crtcs[0]);
        int size = crtc_gamma->size;
        if (size > 0) {
            const double g = 65535.0 / size;
            const double distance = 0.01;
            const double gammag = crtc_gamma->green[1] / g;
            const double gammab = crtc_gamma->blue[1] / g;
            for (int i = 0; i < 20 && temp == -1; i++) {
                // at least second decimal digit is different, given our table
                if ((fabs(whitepoints[i].g - gammag) < distance) && (fabs(whitepoints[i].b - gammab) < distance)) {
                    temp = (i * 500) + 1000;
                }
            }
        }
        
        XFree(crtc_gamma);
    }
    
    XCloseDisplay(dpy);
    return temp;
}

#endif
