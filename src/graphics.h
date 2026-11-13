/*
 * Drawing on the Spectrum's screen, for java.awt.Graphics: the screen is a
 * 256x192 canvas with (0,0) at the top left, as in Java. Drawing sets pixels
 * and the ink of their 8x8 cells; drawing in the background colour clears
 * pixels instead.
 */

#ifndef GRAPHICS_H
#define GRAPHICS_H

extern unsigned char g_ink;     /* the drawing colour, 0 to 7 */
extern unsigned char g_paper;   /* the background colour */

/* The arguments of the drawing operations: corners or a corner and a size */
extern int g_x;
extern int g_y;
extern int g_w;
extern int g_h;

/* Sets the colours of the whole screen from g_paper, for clearing with CLS. */
void g_set_attributes(void);

void g_line(void);              /* from (g_x, g_y) to (g_w, g_h) */
void g_draw_rect(void);
void g_fill_rect(void);
void g_draw_oval(void);
void g_fill_oval(void);

/* Text with its baseline at g_y, in the ROM font */
void g_text(unsigned char *s, unsigned int len);

#endif
