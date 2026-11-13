/*
 * Drawing on the Spectrum's screen, for java.awt.Graphics.
 *
 * Setting pixels is Z80 code: a pixel and a horizontal line, which sets
 * whole bytes. Lines, rectangles, ovals and text are C on top of them, with
 * their locals static, as sccz80 gives shorter code for statics and nothing
 * here is reentrant.
 *
 * Coordinates are ints. The callers keep them between -2048 and 2047, so
 * that nothing below overflows: doubled coordinates, the error terms of
 * lines and the squares in ovals.
 */

#include "graphics.h"

#define CHARS   ((unsigned int *)23606)     /* the ROM font, less 256 */
#define BORDCR  ((unsigned char *)23624)    /* colours of the lower screen */
#define ATTR_P  ((unsigned char *)23693)    /* colours of the upper screen */

unsigned char g_ink;
unsigned char g_paper = 7;

int g_x;
int g_y;
int g_w;
int g_h;

void g_set_attributes(void)
{
    /*
     * Bright, as Java's colours are at full strength, with text in black or
     * white, whichever shows on the paper
     */
    *ATTR_P = 0x40 | g_paper << 3 | (g_paper < 4 ? 7 : 0);
    *BORDCR = *ATTR_P;
#asm
    ld   a,(_g_paper)
    out  ($FE),a        ; the border too, to look like the background
#endasm
}

/* ---- Z80 code: pixels ---- */

#asm
    SECTION code_compiler

; B = the ink, or $FF when drawing in the background colour, which clears pixels
.g_colour
    ld   a,(_g_paper)
    ld   b,a
    ld   a,(_g_ink)
    cp   b
    ld   b,a
    ret  nz
    ld   b,$FF
    ret

; L = x, E = y -> DE = the screen byte; uses A and C
.g_addr
    ld   a,e
    rra
    rra
    rra
    and  $18            ; the third of the screen
    ld   c,a
    ld   a,e
    and  7              ; the pixel line in the character row
    or   c
    or   $40
    ld   d,a
    ld   a,e
    rla
    rla
    and  $E0            ; the character row in the third
    ld   c,a
    ld   a,l
    rra
    rra
    rra
    and  $1F            ; the column
    or   c
    ld   e,a
    ret

; Sets the bits of C in the screen byte at DE and the ink of its cell to B,
; or clears them if B is $FF. Keeps B, DE, HL.
.g_apply
    ld   a,b
    inc  a
    jr   z,g_clear_bits
    ld   a,(de)
    or   c
    ld   (de),a
    push de
    ld   a,d
    rrca
    rrca
    rrca
    and  3
    or   $58            ; the attribute of the cell
    ld   d,a
    ld   a,(de)
    and  $F8
    or   b
    ld   (de),a
    pop  de
    ret
.g_clear_bits
    ld   a,c
    cpl
    ld   c,a
    ld   a,(de)
    and  c
    ld   (de),a
    ret
#endasm

/* Sets a pixel, if it is on the screen. */
static void plot(int x, int y)
{
#asm
    ld   hl,2
    add  hl,sp
    ld   e,(hl)
    inc  hl
    ld   d,(hl)         ; y
    inc  hl
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a            ; x
.plot_xy                ; for the Z80 code: HL = x, DE = y; uses all registers
    ld   a,h
    or   d
    ret  nz             ; x or y is negative or over 255
    ld   a,e
    cp   192
    ret  nc
    call g_colour
    call g_addr
    ld   a,l
    and  7
    ld   h,a
    ld   a,$80
    jr   z,plot_mask
.plot_shift
    rrca
    dec  h
    jr   nz,plot_shift
.plot_mask
    ld   c,a
    jp   g_apply
#endasm
}

/* Sets the pixels from x0 to x1 on line y, all on the screen, with x0 <= x1. */
static void hline(int x0, int x1, int y)
{
#asm
    ld   hl,2
    add  hl,sp
    ld   e,(hl)         ; y
    inc  hl
    inc  hl
    ld   d,(hl)         ; x1
    inc  hl
    inc  hl
    ld   l,(hl)         ; x0
    ld   h,d
    call g_colour
    call g_addr
    ld   a,l
    rrca
    rrca
    rrca
    and  $1F
    ld   c,a
    ld   a,h
    rrca
    rrca
    rrca
    and  $1F
    sub  c
    ld   c,a            ; the bytes after the first
    ld   a,h
    and  7
    ld   h,$80
    jr   z,hline_last
.hline_last_shift
    sra  h
    dec  a
    jr   nz,hline_last_shift
.hline_last
    ld   a,l            ; H = the bits of the last byte
    and  7
    ld   l,c
    ld   c,$FF
    jr   z,hline_first
.hline_first_shift
    srl  c
    dec  a
    jr   nz,hline_first_shift
.hline_first
    ld   a,l            ; C = the bits of the first byte, L = the bytes after it
    or   a
    jr   nz,hline_bytes
    ld   a,c
    and  h
    ld   c,a
    jp   g_apply
.hline_bytes
    call g_apply
.hline_next
    inc  e
    dec  l
    jr   z,hline_end
    ld   c,$FF
    call g_apply
    jr   hline_next
.hline_end
    ld   c,h
    jp   g_apply
#endasm
}

/* ---- Shapes ---- */

/* Sets the pixels from x0 to x1 on line y that are on the screen. */
static void span(int x0, int x1, int y)
{
    if (y < 0 || y > 191)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 > 255)
        x1 = 255;
    if (x0 <= x1)
        hline(x0, x1, y);
}

/* The state of a line for line_loop, after the position in g_x and g_y */
int ln_n;               /* pixels after this one */
int ln_err;
int ln_add;             /* added to ln_err at each pixel */
int ln_sub;             /* and taken off at each step along the minor axis */
int ln_mx;              /* the step along the major axis */
int ln_my;
int ln_nx;              /* along the minor axis */
int ln_ny;

static void line_loop(void)
{
#asm
.line_pixel
    ld   hl,(_g_x)
    ld   de,(_g_y)
    call plot_xy
    ld   hl,(_ln_n)
    ld   a,h
    or   l
    ret  z
    dec  hl
    ld   (_ln_n),hl
    ld   hl,(_g_x)
    ld   de,(_ln_mx)
    add  hl,de
    ld   (_g_x),hl
    ld   hl,(_g_y)
    ld   de,(_ln_my)
    add  hl,de
    ld   (_g_y),hl
    ld   hl,(_ln_err)
    ld   de,(_ln_add)
    add  hl,de
    bit  7,h
    jr   nz,line_same       ; still negative: no step along the minor axis
    ld   de,(_ln_sub)
    and  a
    sbc  hl,de
    ld   (_ln_err),hl
    ld   hl,(_g_x)
    ld   de,(_ln_nx)
    add  hl,de
    ld   (_g_x),hl
    ld   hl,(_g_y)
    ld   de,(_ln_ny)
    add  hl,de
    ld   (_g_y),hl
    jr   line_pixel
.line_same
    ld   (_ln_err),hl
    jr   line_pixel
#endasm
}

/*
 * Bresenham's line, with the same rule as Java2D for which pixel to take
 * when the line passes halfway between two
 */
void g_line(void)
{
    static int dx;
    static int dy;
    static int sx;
    static int sy;

    dx = g_w - g_x;
    sx = 1;
    if (dx < 0) {
        dx = -dx;
        sx = -1;
    }
    dy = g_h - g_y;
    sy = 1;
    if (dy < 0) {
        dy = -dy;
        sy = -1;
    }
    ln_mx = ln_my = ln_nx = ln_ny = 0;
    if (dx >= dy) {
        ln_mx = sx;
        ln_ny = sy;
        ln_n = dx;
        ln_add = dy + dy;
    } else {
        ln_my = sy;
        ln_nx = sx;
        ln_n = dy;
        ln_add = dx + dx;
    }
    ln_err = -ln_n;
    ln_sub = ln_n + ln_n;
    line_loop();
}

/* As Java's: covers g_w + 1 by g_h + 1 pixels */
void g_draw_rect(void)
{
    static int x1;
    static int y;

    if (g_w < 0 || g_h < 0)
        return;
    x1 = g_x + g_w;
    span(g_x, x1, g_y);
    span(g_x, x1, g_y + g_h);
    for (y = g_y + 1; y < g_y + g_h; y++) {
        plot(g_x, y);
        plot(x1, y);
    }
}

/* Covers g_w by g_h pixels */
void g_fill_rect(void)
{
    static int x1;

    if (g_w < 1 || g_h < 1)
        return;
    x1 = g_x + g_w - 1;
    g_h += g_y;                 /* the line after the last */
    if (g_y < 0)
        g_y = 0;
    if (g_h > 192)
        g_h = 192;
    for (; g_y < g_h; g_y++)
        span(g_x, x1, g_y);
}

static unsigned long isqrt_v;
static unsigned int isqrt_root;

/* HL = the integer square root of isqrt_v, found two bits at a time; uses all registers */
#asm
    SECTION code_compiler
.isqrt
    exx                 ; v in the other registers, keeping BASIC's HL'
    push hl
    push de
    ld   de,(_isqrt_v)
    ld   hl,(_isqrt_v+2)    ; HL'DE' = v
    exx
    ld   hl,0
    ld   (_isqrt_root),hl
    ld   c,l            ; CHL = the remainder
    ld   b,16
.isqrt_bits
    exx
    sla  e
    rl   d
    rl   l
    rl   h
    exx
    adc  hl,hl
    rl   c
    exx
    sla  e
    rl   d
    rl   l
    rl   h
    exx
    adc  hl,hl
    rl   c              ; the next two bits of v into the remainder
    ld   de,(_isqrt_root)
    sla  e
    rl   d
    ld   (_isqrt_root),de
    ld   a,0
    sla  e
    rl   d
    rla
    inc  e              ; ADE = 2 * root + 1, to try
    push hl
    and  a
    sbc  hl,de
    ld   e,a
    ld   a,c
    sbc  a,e
    jr   c,isqrt_less
    ld   c,a            ; it fits: keep the smaller remainder
    pop  de
    ld   de,(_isqrt_root)
    inc  e
    ld   (_isqrt_root),de
    djnz isqrt_bits
    jr   isqrt_done
.isqrt_less
    pop  hl
    djnz isqrt_bits
.isqrt_done
    exx
    pop  de
    pop  hl
    exx
    ld   hl,(_isqrt_root)
    ret
#endasm

/*
 * The ellipse being drawn, a line at a time. It is size across and other
 * along the other axis, and t is the distance of the line from its middle
 * along that axis; t and the middle across are doubled, so that they can be
 * halfway between pixels.
 */
static int oval_middle;
static int oval_size;
static int oval_other;
static unsigned long oval_ratio;    /* size / other, with 12 bits after the point */
static unsigned long oval_left;     /* other * other - t * t */
static int oval_step;               /* what oval_left grows by at the next line */

static int oval_floor;              /* the edge rounded down, set by oval_edge */

/* Starts at the first line, where t is -other. */
static void oval_start(int middle, int size, int other)
{
    oval_middle = middle;
    oval_size = size;
    oval_other = other;
    if (other != 0)
        oval_ratio = ((unsigned long)size << 12) / (unsigned int)other;
    oval_left = 0;
    oval_step = 4 * other - 4;     /* t * t - (t + 2) * (t + 2) */
}

/*
 * Returns the edge across the ellipse on this line, rounded down after adding
 * round 32nds of a pixel, and leaves it rounded down in oval_floor. Then moves
 * to the next line.
 *
 * The edge is middle / 2 - w, where the doubled width 2w is
 * size * sqrt(other * other - t * t) / other. Worked out in 16ths of a pixel,
 * 2w fits 16 bits, and as size / other has 12 bits after the point, the
 * product 28 bits.
 */
static int oval_edge(int round)
{
#asm
    ld   hl,(_oval_other)
    ld   a,h
    or   l
    jr   nz,oval_curve
    ld   hl,(_oval_size)    ; a line: the full width
    add  hl,hl
    add  hl,hl
    add  hl,hl
    add  hl,hl
    jr   oval_width
.oval_curve
    xor  a
    ld   (_isqrt_v),a
    ld   hl,(_oval_left)
    ld   (_isqrt_v+1),hl
    ld   a,(_oval_left+2)
    ld   (_isqrt_v+3),a     ; oval_left * 256, for 16ths of a pixel
    call isqrt
    push ix
    push hl
    pop  ix                 ; IX = the root, multiplied by the ratio a bit at a time
    xor  a
    ld   h,a
    ld   l,a
    ld   d,a
    ld   e,a                ; ADEHL = the product
    ld   b,16
.oval_mul
    add  hl,hl
    ex   de,hl
    adc  hl,hl
    ex   de,hl
    adc  a,a
    add  ix,ix
    jr   nc,oval_mul_next
    push bc
    ld   bc,(_oval_ratio)
    add  hl,bc
    ld   bc,(_oval_ratio+2)
    ex   de,hl
    adc  hl,bc
    ex   de,hl
    adc  a,0
    pop  bc
.oval_mul_next
    djnz oval_mul
    pop  ix
    ld   l,h
    ld   h,e
    ld   a,d
    ld   b,4
.oval_mul_shift
    srl  a
    rr   h
    rr   l
    djnz oval_mul_shift     ; HL = the product / 4096 = 2w
.oval_width
    ex   de,hl
    ld   hl,(_oval_middle)
    ld   bc,8192            ; offset by 4096 pixels, to stay positive
    add  hl,bc
    xor  a
    add  hl,hl
    adc  a,a
    add  hl,hl
    adc  a,a
    add  hl,hl
    adc  a,a
    add  hl,hl
    adc  a,a
    and  a
    sbc  hl,de
    sbc  a,0                ; AHL = the edge, in 32nds of a pixel
    push af
    push hl
    call oval_pixels
    ld   (_oval_floor),hl
    pop  hl
    pop  af
    ex   de,hl
    ld   hl,2
    add  hl,sp
    ld   c,(hl)
    inc  hl
    ld   b,(hl)             ; round
    ex   de,hl
    add  hl,bc
    adc  a,0
    call oval_pixels
    push hl
    ld   de,(_oval_step)    ; the next line
    ld   a,d
    rla
    sbc  a,a
    ld   c,a                ; the sign of the step
    ld   hl,(_oval_left)
    add  hl,de
    ld   (_oval_left),hl
    ld   hl,(_oval_left+2)
    ld   e,c
    ld   d,c
    adc  hl,de
    ld   (_oval_left+2),hl
    ld   hl,(_oval_step)
    ld   de,-8
    add  hl,de
    ld   (_oval_step),hl
    pop  hl
    ret

; HL = AHL / 32 - 4096, from 32nds of a pixel offset by 4096 pixels to pixels
.oval_pixels
    ld   b,5
.oval_pixels_shift
    srl  a
    rr   h
    rr   l
    djnz oval_pixels_shift
    ld   de,-4096
    add  hl,de
    ret
#endasm
}

static unsigned char swapped;   /* x and y are swapped for plot4 */

/* Sets the pixels at x0 and x1 on lines y0 and y1. */
static void plot4(int x0, int x1, int y0, int y1)
{
    if (swapped) {
        plot(y0, x0);
        plot(y0, x1);
        plot(y1, x0);
        plot(y1, x1);
    } else {
        plot(x0, y0);
        plot(x1, y0);
        plot(x0, y1);
        plot(x1, y1);
    }
}

/*
 * As Java's, the outline passes through the centres of the pixels at the
 * edges of g_w + 1 by g_h + 1 pixels. Each line and each column gets the
 * pixels nearest to the ellipse, which leaves no gaps; a quarter is
 * calculated and mirrored. Java2D draws the ellipse as Bezier curves, so its
 * pixels differ in places; rounding 15/32 down rather than 16/32 makes them
 * agree a little more often.
 */
void g_draw_oval(void)
{
    static int i;
    static int a;
    static int t;

    if (g_w < 0 || g_h < 0)
        return;
    /* The lines, then the columns, as lines with x and y swapped */
    for (swapped = 0; swapped < 2; swapped++) {
        oval_start(g_x + g_x + g_w, g_w, g_h);
        for (i = 0; i + i <= g_h; i++) {
            a = oval_edge(15);
            plot4(a, g_x + g_x + g_w - a, g_y + i, g_y + g_h - i);
        }
        t = g_x;
        g_x = g_y;
        g_y = t;
        t = g_w;
        g_w = g_h;
        g_h = t;
    }
}

/*
 * As Java2D: the pixels whose top left corners are inside the ellipse, or on
 * its left edge
 */
void g_fill_oval(void)
{
    static int i;
    static int a;
    static int b;

    if (g_w < 1 || g_h < 1)
        return;
    oval_start(g_x + g_x + g_w, g_w, g_h);
    oval_edge(0);                   /* the first line is empty */
    for (i = 1; i + i <= g_h; i++) {
        a = oval_edge(31);                                          /* rounded up */
        b = g_x + g_x + g_w - 1 - oval_floor;                       /* mirrored, rounded down */
        span(a, b, g_y + i);
        span(a, b, g_y + g_h - i);
    }
}

static unsigned char *glyph_row;
static int glyph_y;

/* Sets the pixels of an 8x8 glyph, with its top left corner at g_x, g_y - 7. */
static void glyph(unsigned char *p)
{
#asm
    ld   hl,2
    add  hl,sp
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a
    ld   (_glyph_row),hl
    ld   hl,(_g_y)
    ld   de,-7              ; the baseline is under the seventh row
    add  hl,de
    ld   (_glyph_y),hl
    ld   b,8
.glyph_rows
    push bc
    ld   hl,(_glyph_row)
    ld   a,(hl)
    inc  hl
    ld   (_glyph_row),hl
    ld   hl,(_g_x)
.glyph_bits
    or   a
    jr   z,glyph_next       ; no more pixels in the row
    add  a,a
    jr   nc,glyph_skip
    push af
    push hl
    ld   de,(_glyph_y)
    call plot_xy
    pop  hl
    pop  af
.glyph_skip
    inc  hl
    jr   glyph_bits
.glyph_next
    ld   hl,(_glyph_y)
    inc  hl
    ld   (_glyph_y),hl
    pop  bc
    djnz glyph_rows
#endasm
}

/* Printable ASCII as itself and other characters as ?, as print_chars in zxjvm.c */
void g_text(unsigned char *s, unsigned int len)
{
    static unsigned char c;

    while (len-- != 0) {
        c = *s++;
        if ((c & 0xC0) == 0x80)     /* a continuation byte */
            continue;
        if (c < 32 || c > 126)
            c = '?';
        glyph((unsigned char *)(*CHARS + 8 * c));
        g_x += 8;
    }
}
