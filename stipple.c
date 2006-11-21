#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include "db.h"
#include "xwin.h"

#define STIPW 8
#define STIPH 8 
#define MAXSTIP 100

extern Display *dpy; 
extern int scr;
extern Window win;

Pixmap stipple[MAXSTIP];


static char stip_bits[] = {	/* 8x diagonal crosshatch */
    0x82, 0x44, 0x28, 0x10,
    0x28, 0x44, 0x82, 0x01
};

char stip_src[][STIPW] = {

    /* (EQU :F2) family of ten 45 degree stipples, each offset */
    /* by one pixel.  last two are doubled */

    {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}, /* 8x+0  45d */
    {0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02}, /* 8x+1  45d */
    {0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08, 0x04}, /* 8x+2  45d */
    {0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08}, /* 8x+3  45d */
    {0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10}, /* 8x+4  45d */
    {0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40, 0x20}, /* 8x+5  45d */
    {0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40}, /* 8x+6  45d */
    {0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80}, /* 8x+7  45d */
    {0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80, 0x40}, /* 8x+6  45d */
    {0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x80}, /* 8x+7  45d */

    /* (EQU :F3) family of ten 135 degree stipples, each offset */
    /* by one pixel.  last two are doubled */

    {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}, /* 8x+0 135d */
    {0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40}, /* 8x+1 135d */
    {0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20}, /* 8x+2 135d */
    {0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x10}, /* 8x+3 135d */
    {0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08}, /* 8x+4 135d */
    {0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04}, /* 8x+5 135d */
    {0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02}, /* 8x+6 135d */
    {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01}, /* 8x+7 135d */
    {0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01, 0x02}, /* 8x+6 135d */
    {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x01}, /* 8x+7 135d */

    /* (EQU :F4) family of ten 4x4 single point stipples */

    {0x11, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00}, /* 4x4,1  */
    {0x44, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00}, /* 4x4,2  */
    {0x88, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00}, /* 4x4,3  */
    {0x00, 0x11, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00}, /* 4x4,4  */
    {0x00, 0x22, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00}, /* 4x4,5  */
    {0x00, 0x44, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00}, /* 4x4,6  */
    {0x00, 0x88, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00}, /* 4x4,7  */
    {0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x44, 0x00}, /* 4x4,8  */
    {0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x88, 0x00}, /* 4x4,9  */
    {0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x11, 0x00}, /* 4x4,10 */

    /* (EQU :F5) family of ten horizontal period 8 stipples */

    {0x11, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00}, /* 4x4,1  */
    {0x44, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00}, /* 4x4,2  */
    {0x88, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00}, /* 4x4,3  */
    {0x00, 0x11, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00}, /* 4x4,4  */
    {0x00, 0x22, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00}, /* 4x4,5  */
    {0x00, 0x44, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00}, /* 4x4,6  */
    {0x00, 0x88, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00}, /* 4x4,7  */
    {0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x11, 0x00}, /* 4x4,8  */
    {0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x22, 0x00}, /* 4x4,9  */
    {0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x44, 0x00}, /* 4x4,10 */


    /* (EQU :F6) family of ten horizontal period 8 stipples */
    {0xf0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00},
    {0x00, 0xf0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00},
    {0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x0f, 0x00},
    {0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x0f},
    {0x0f, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00},
    {0x00, 0x0f, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00},
    {0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0xf0, 0x00},
    {0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0xf0},
    {0xf0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00},
    {0x00, 0xf0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00}
};

void init_stipples() {
    int i,j;
    unsigned char a;
    for (i=0; i<50; i++) {	/* deterministic stipples */
	stipple[i] = XCreateBitmapFromData(dpy, win, stip_src[i], 
	    (unsigned int) STIPW, (unsigned int) STIPH);
    }
    for (; i<MAXSTIP; i++) {	/* make random stipples */
    	for (j=0; j<STIPW; j++) {
	    a = (unsigned char) (drand48()*255.0);
	    a &= (unsigned char) (drand48()*255.0);
	    a &= (unsigned char) (drand48()*255.0);
	    a &= (unsigned char) (drand48()*255.0);
	    /* a &= (unsigned char) (drand48()*255.0); */
	    stip_bits[j] = (unsigned char) a;
	}
	stipple[i] = XCreateBitmapFromData(dpy, win, stip_bits, 
	    (unsigned int) STIPW, (unsigned int) STIPH);
    }
}
