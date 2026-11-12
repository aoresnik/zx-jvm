/*
 * IEEE 754 binary64 arithmetic in software; see double.h.
 *
 * The algorithms follow Berkeley SoftFloat: operands are unpacked into a sign,
 * an exponent and a 64-bit significand with spare low bits, the operation is
 * done exactly or with the lost bits ORed into bit 0 ("jamming"), and the
 * result is rounded once when it is packed again. sccz80 has no 64-bit type,
 * so significands are two 32-bit words, the high word first.
 */

#include "double.h"

#define KIND_ZERO   0
#define KIND_FINITE 1
#define KIND_INF    2
#define KIND_NAN    3

#define SIGN_BIT 0x80000000UL
#define INF_HIGH 0x7FF00000UL

/* value = sig * 2^(exp - 63), with bit 63 of sig set when the kind is KIND_FINITE */
struct unpacked {
    unsigned char sign;
    unsigned char kind;
    int exp;
    uint32_t sig[2];
};

/* ---- 64-bit significands ---- */

/*
 * shl1:    x <<= 1
 * shr_jam: x >>= n, keeping any lost bits as bit 0 so that rounding still sees them
 * add64:   a += b
 * sub64:   a -= b
 * less64:  a < b
 * mul64:   a = the high 64 bits of a * b, jammed
 * div64:   q = the 64-bit quotient n / d, for d <= n < 2d, with the remainder left in n
 */

#ifdef __SCCZ80

/*
 * In Z80 code, as sccz80 calls a library routine for every 32-bit operation;
 * this makes a multiplication about five times faster. A significand is the
 * high word then the low word, each little-endian, so its bytes from the
 * least significant are at offsets 4, 5, 6, 7, 0, 1, 2, 3. Arguments are on
 * the stack, the last one nearest the return address, except that a fastcall
 * argument is in HL. IY must be kept, for the ROM's interrupt routine.
 */

static uint32_t mul_acc[2];
static uint32_t *mul_b;
static unsigned char mul_bytes;
static unsigned char mul_lost;
static uint32_t div_d[2];
static uint32_t div_q[2];

static void shl1(uint32_t *x) __z88dk_fastcall
{
#asm
    push hl
    ld   de,4
    add  hl,de
    sla  (hl)           ; the low word
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
    pop  hl             ; keeps the carry
    rl   (hl)           ; the high word
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
#endasm
}

static void shr_jam(uint32_t *x, int n)
{
#asm
    push ix
    ld   hl,4
    add  hl,sp
    ld   c,(hl)
    inc  hl
    ld   b,(hl)         ; bc = n
    inc  hl
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a
    push hl
    pop  ix             ; ix = x
    ld   e,0            ; e: bits lost, ORed
    ld   a,b
    or   a
    jr   nz,shr_all     ; n >= 256
    ld   a,c
    cp   64
    jr   nc,shr_all
.shr_bytes              ; whole bytes first
    cp   8
    jr   c,shr_bits
    ld   a,e
    or   (ix+4)
    ld   e,a
    ld   a,(ix+5)
    ld   (ix+4),a
    ld   a,(ix+6)
    ld   (ix+5),a
    ld   a,(ix+7)
    ld   (ix+6),a
    ld   a,(ix+0)
    ld   (ix+7),a
    ld   a,(ix+1)
    ld   (ix+0),a
    ld   a,(ix+2)
    ld   (ix+1),a
    ld   a,(ix+3)
    ld   (ix+2),a
    ld   (ix+3),0
    ld   a,c
    sub  8
    ld   c,a
    jr   shr_bytes
.shr_bits
    or   a
    jr   z,shr_sticky
    ld   b,a
.shr_bit
    srl  (ix+3)
    rr   (ix+2)
    rr   (ix+1)
    rr   (ix+0)
    rr   (ix+7)
    rr   (ix+6)
    rr   (ix+5)
    rr   (ix+4)
    jr   nc,shr_kept
    ld   e,1
.shr_kept
    djnz shr_bit
    jr   shr_sticky
.shr_all
    ld   b,8
    push ix
    pop  hl
.shr_clear
    ld   a,e
    or   (hl)
    ld   e,a
    ld   (hl),0
    inc  hl
    djnz shr_clear
.shr_sticky
    ld   a,e
    or   a
    jr   z,shr_done
    set  0,(ix+4)
.shr_done
    pop  ix
#endasm
}

static void add64(uint32_t *a, uint32_t *b)
{
#asm
    ld   hl,2
    add  hl,sp
    ld   e,(hl)
    inc  hl
    ld   d,(hl)         ; de = b
    inc  hl
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a            ; hl = a
    push hl
    push de
    ld   bc,4
    add  hl,bc
    ex   de,hl
    add  hl,bc
    ex   de,hl
    ld   a,(de)         ; the low words
    add  a,(hl)
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
    pop  de             ; the high words; pop keeps the carry
    pop  hl
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
#endasm
}

static void sub64(uint32_t *a, uint32_t *b)
{
#asm
    ld   hl,2
    add  hl,sp
    ld   e,(hl)
    inc  hl
    ld   d,(hl)         ; de = b
    inc  hl
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a            ; hl = a
    push hl
    push de
    ld   bc,4
    add  hl,bc
    ex   de,hl
    add  hl,bc
    ex   de,hl
    ld   a,(de)         ; the low words
    ld   c,a
    ld   a,(hl)
    sub  c
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   (hl),a
    pop  de             ; the high words; pop keeps the carry
    pop  hl
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   (hl),a
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   (hl),a
#endasm
}

static int less64(uint32_t *a, uint32_t *b)
{
#asm
    ld   hl,2
    add  hl,sp
    ld   e,(hl)
    inc  hl
    ld   d,(hl)         ; de = b
    inc  hl
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a            ; hl = a
    push hl
    push de
    ld   bc,4
    add  hl,bc
    ex   de,hl
    add  hl,bc
    ex   de,hl
    ld   a,(de)         ; a - b, keeping only the borrow
    ld   c,a
    ld   a,(hl)
    sub  c
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    pop  de
    pop  hl
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    inc  hl
    inc  de
    ld   a,(de)
    ld   c,a
    ld   a,(hl)
    sbc  a,c
    ld   hl,0
    rl   l              ; the borrow
#endasm
}

/*
 * Shift and add: for each bit of b from the lowest, add a to the high half of
 * the product if the bit is set, then shift the product right. After 64 bits
 * the high half is in mul_acc, and mul_lost says whether the bits shifted out
 * of it were all zero.
 */
static void mul64(uint32_t *a, uint32_t *b)
{
#asm
    push ix
    ld   hl,4
    add  hl,sp
    ld   e,(hl)
    inc  hl
    ld   d,(hl)         ; de = b
    inc  hl
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a
    push hl
    pop  ix             ; ix = a
    ld   (_mul_b),de
    ld   hl,_mul_acc
    ld   b,8
.mul_clear
    ld   (hl),0
    inc  hl
    djnz mul_clear
    xor  a
    ld   (_mul_lost),a
    ld   hl,4
    add  hl,de
    ex   de,hl          ; de: the next byte of b, from b + 4
    ld   a,8
.mul_byte
    ld   (_mul_bytes),a
    ld   a,(de)
    ld   c,a
    inc  de
    ld   b,8
.mul_bit
    srl  c
    jr   nc,mul_shift   ; with the carry clear
    ld   hl,_mul_acc+4  ; mul_acc += a, the low words first
    ld   a,(ix+4)
    add  a,(hl)
    ld   (hl),a
    inc  hl
    ld   a,(ix+5)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    ld   a,(ix+6)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    ld   a,(ix+7)
    adc  a,(hl)
    ld   (hl),a
    ld   hl,_mul_acc
    ld   a,(ix+0)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    ld   a,(ix+1)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    ld   a,(ix+2)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    ld   a,(ix+3)
    adc  a,(hl)
    ld   (hl),a
.mul_shift              ; shift the carry and mul_acc right
    ld   hl,_mul_acc+3
    rr   (hl)
    dec  hl
    rr   (hl)
    dec  hl
    rr   (hl)
    dec  hl
    rr   (hl)
    ld   hl,_mul_acc+7
    rr   (hl)
    dec  hl
    rr   (hl)
    dec  hl
    rr   (hl)
    dec  hl
    rr   (hl)
    jr   nc,mul_kept
    ld   a,1
    ld   (_mul_lost),a
.mul_kept
    djnz mul_bit
    ld   a,(_mul_bytes)
    dec  a
    jr   z,mul_done
    cp   4
    jr   nz,mul_byte
    ld   de,(_mul_b)    ; after b + 7 comes b + 0
    jr   mul_byte
.mul_done
    push ix
    pop  de
    ld   hl,_mul_acc
    ld   bc,8
    ldir
    ld   a,(_mul_lost)
    or   a
    jr   z,mul_exact
    set  0,(ix+4)
.mul_exact
    pop  ix
#endasm
}

/*
 * Restoring division, one quotient bit at a time: if n >= d, subtract d and
 * set the bit; then shift n left. d and the quotient are kept in div_d and
 * div_q, so that n is the only pointer needed.
 */
static void div64(uint32_t *n, uint32_t *d, uint32_t *q)
{
#asm
    push ix
    ld   hl,6
    add  hl,sp
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a            ; hl = d
    ld   de,_div_d
    ld   bc,8
    ldir
    ld   hl,8
    add  hl,sp
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a
    push hl
    pop  ix             ; ix = n
    ld   hl,_div_q
    ld   b,8
.div_clear
    ld   (hl),0
    inc  hl
    djnz div_clear
    ld   b,64
.div_bit
    ld   hl,_div_q+4    ; div_q <<= 1
    sla  (hl)
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
    ld   hl,_div_q
    rl   (hl)
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
    inc  hl
    rl   (hl)
    ld   hl,_div_d+3    ; compare n with d, from the most significant byte
    ld   a,(ix+3)
    cp   (hl)
    jr   nz,div_compared
    dec  hl
    ld   a,(ix+2)
    cp   (hl)
    jr   nz,div_compared
    dec  hl
    ld   a,(ix+1)
    cp   (hl)
    jr   nz,div_compared
    dec  hl
    ld   a,(ix+0)
    cp   (hl)
    jr   nz,div_compared
    ld   hl,_div_d+7
    ld   a,(ix+7)
    cp   (hl)
    jr   nz,div_compared
    dec  hl
    ld   a,(ix+6)
    cp   (hl)
    jr   nz,div_compared
    dec  hl
    ld   a,(ix+5)
    cp   (hl)
    jr   nz,div_compared
    dec  hl
    ld   a,(ix+4)
    cp   (hl)
.div_compared
    jr   c,div_shift    ; n < d
    ld   hl,_div_d+4    ; n -= d
    ld   a,(ix+4)
    sub  (hl)
    ld   (ix+4),a
    inc  hl
    ld   a,(ix+5)
    sbc  a,(hl)
    ld   (ix+5),a
    inc  hl
    ld   a,(ix+6)
    sbc  a,(hl)
    ld   (ix+6),a
    inc  hl
    ld   a,(ix+7)
    sbc  a,(hl)
    ld   (ix+7),a
    ld   hl,_div_d
    ld   a,(ix+0)
    sbc  a,(hl)
    ld   (ix+0),a
    inc  hl
    ld   a,(ix+1)
    sbc  a,(hl)
    ld   (ix+1),a
    inc  hl
    ld   a,(ix+2)
    sbc  a,(hl)
    ld   (ix+2),a
    inc  hl
    ld   a,(ix+3)
    sbc  a,(hl)
    ld   (ix+3),a
    ld   hl,_div_q+4
    set  0,(hl)
.div_shift              ; n <<= 1
    sla  (ix+4)
    rl   (ix+5)
    rl   (ix+6)
    rl   (ix+7)
    rl   (ix+0)
    rl   (ix+1)
    rl   (ix+2)
    rl   (ix+3)
    dec  b
    jp   nz,div_bit
    ld   hl,4
    add  hl,sp
    ld   e,(hl)
    inc  hl
    ld   d,(hl)         ; de = q
    ld   hl,_div_q
    ld   bc,8
    ldir
    pop  ix
#endasm
}

#else

static void shl1(uint32_t *x)
{
    x[0] = (x[0] << 1) | (x[1] >> 31);
    x[1] <<= 1;
}

static void shr_jam(uint32_t *x, int n)
{
    uint32_t lost;

    lost = 0;
    if (n >= 64) {
        lost = x[0] | x[1];
        x[0] = 0;
        x[1] = 0;
    } else {
        if (n >= 32) {
            lost = x[1];
            x[1] = x[0];
            x[0] = 0;
            n -= 32;
        }
        while (n-- > 0) {
            lost |= x[1] & 1;
            x[1] = (x[1] >> 1) | (x[0] << 31);
            x[0] >>= 1;
        }
    }
    if (lost != 0)
        x[1] |= 1;
}

static void add64(uint32_t *a, uint32_t *b)
{
    uint32_t lo;

    lo = a[1] + b[1];
    a[0] = a[0] + b[0] + (lo < a[1] ? 1 : 0);
    a[1] = lo;
}

static void sub64(uint32_t *a, uint32_t *b)
{
    uint32_t borrow;

    borrow = a[1] < b[1] ? 1 : 0;
    a[1] = a[1] - b[1];
    a[0] = a[0] - b[0] - borrow;
}

static int less64(uint32_t *a, uint32_t *b)
{
    return a[0] < b[0] || (a[0] == b[0] && a[1] < b[1]);
}

/* Multiplies 16-bit pieces, as the Z80 version's shift and add would be slow in C */
static void mul64(uint32_t *a, uint32_t *b)
{
    uint16_t x[4];
    uint16_t y[4];
    uint16_t p[8];
    uint32_t t;
    unsigned char i;
    unsigned char j;

    for (i = 0; i < 2; i++) {           /* least significant piece first */
        x[2 * i] = (uint16_t)a[1 - i];
        x[2 * i + 1] = (uint16_t)(a[1 - i] >> 16);
        y[2 * i] = (uint16_t)b[1 - i];
        y[2 * i + 1] = (uint16_t)(b[1 - i] >> 16);
    }
    for (i = 0; i < 8; i++)
        p[i] = 0;
    for (i = 0; i < 4; i++) {
        t = 0;
        for (j = 0; j < 4; j++) {
            t = (uint32_t)x[i] * y[j] + p[i + j] + (t >> 16);   /* at most 2^32 - 1 */
            p[i + j] = (uint16_t)t;
        }
        p[i + 4] = (uint16_t)(t >> 16);
    }
    a[0] = ((uint32_t)p[7] << 16) | p[6];
    a[1] = ((uint32_t)p[5] << 16) | p[4];
    if ((p[0] | p[1] | p[2] | p[3]) != 0)
        a[1] |= 1;
}

static void div64(uint32_t *n, uint32_t *d, uint32_t *q)
{
    unsigned char i;

    q[0] = 0;
    q[1] = 0;
    for (i = 0; i < 64; i++) {
        shl1(q);
        if (!less64(n, d)) {
            sub64(n, d);
            q[1] |= 1;
        }
        shl1(n);
    }
}

#endif

/* ---- Packing and unpacking ---- */

/* Shifts a non-zero significand left until bit 63 is set. */
static void normalize(struct unpacked *u)
{
    if (u->sig[0] == 0) {
        u->sig[0] = u->sig[1];
        u->sig[1] = 0;
        u->exp -= 32;
    }
    while ((u->sig[0] & SIGN_BIT) == 0) {
        shl1(u->sig);
        u->exp--;
    }
}

static void unpack(struct unpacked *u, uint32_t *d)
{
    unsigned int e;

    u->sign = (d[0] & SIGN_BIT) != 0;
    e = (unsigned int)(d[0] >> 20) & 0x7FF;
    u->sig[0] = d[0] & 0xFFFFFUL;
    u->sig[1] = d[1];
    if (e == 0x7FF) {
        u->kind = (u->sig[0] | u->sig[1]) != 0 ? KIND_NAN : KIND_INF;
        return;
    }
    u->kind = KIND_FINITE;
    if (e == 0) {
        if ((u->sig[0] | u->sig[1]) == 0) {
            u->kind = KIND_ZERO;
            return;
        }
        u->exp = -1011;                 /* a subnormal is sig * 2^-1074 */
        normalize(u);
        return;
    }
    u->sig[0] = ((u->sig[0] | 0x100000UL) << 11) | (u->sig[1] >> 21);
    u->sig[1] <<= 11;
    u->exp = (int)e - 1023;
}

/* Rounds a non-zero finite value to the nearest double, ties to even, and packs it. */
static void pack(uint32_t *r, struct unpacked *u)
{
    uint32_t *sig;
    uint32_t round_bits;
    int e;

    normalize(u);
    sig = u->sig;
    e = u->exp + 1023;                  /* the biased exponent, if the result is normal */
    if (e < 1) {                        /* not e <= 0: sccz80 tests that with a stale carry flag */
        shr_jam(sig, 1 - e);            /* subnormal: align to the smallest exponent */
        e = 1;
    }
    round_bits = sig[1] & 0x7FF;        /* the 11 bits below the 53 that are kept */
    sig[1] = (sig[1] >> 11) | (sig[0] << 21);
    sig[0] >>= 11;
    if (round_bits > 0x400 || (round_bits == 0x400 && (sig[1] & 1) != 0)) {
        if (++sig[1] == 0)
            sig[0]++;
        if (sig[0] == 0x200000UL) {     /* rounded up to the next power of two */
            sig[0] = 0x100000UL;
            e++;
        }
    }
    if (e >= 0x7FF) {
        r[0] = ((uint32_t)u->sign << 31) | INF_HIGH;
        r[1] = 0;
        return;
    }
    if ((sig[0] & 0x100000UL) == 0)
        e = 0;                          /* subnormal */
    r[0] = ((uint32_t)u->sign << 31) | ((uint32_t)e << 20) | (sig[0] & 0xFFFFFUL);
    r[1] = sig[1];
}

/* Zero (high = 0) or infinity (high = INF_HIGH) with a sign */
static void special(uint32_t *r, unsigned char sign, uint32_t high)
{
    r[0] = ((uint32_t)sign << 31) | high;
    r[1] = 0;
}

static void make_nan(uint32_t *r)
{
    r[0] = 0x7FF80000UL;
    r[1] = 0;
}

/* ---- Operations ---- */

static void add_signed(uint32_t *r, uint32_t *a, uint32_t *b, unsigned char negate_b)
{
    struct unpacked x;
    struct unpacked y;
    struct unpacked *p;
    struct unpacked *q;

    unpack(&x, a);
    unpack(&y, b);
    y.sign ^= negate_b;
    if (x.kind == KIND_NAN || y.kind == KIND_NAN) {
        make_nan(r);
        return;
    }
    if (x.kind == KIND_INF) {
        if (y.kind == KIND_INF && x.sign != y.sign)
            make_nan(r);
        else
            special(r, x.sign, INF_HIGH);
        return;
    }
    if (y.kind == KIND_INF) {
        special(r, y.sign, INF_HIGH);
        return;
    }
    if (y.kind == KIND_ZERO) {
        if (x.kind == KIND_ZERO)
            special(r, x.sign & y.sign, 0);
        else {
            r[0] = a[0];
            r[1] = a[1];
        }
        return;
    }
    if (x.kind == KIND_ZERO) {
        r[0] = ((uint32_t)y.sign << 31) | (b[0] & ~SIGN_BIT);
        r[1] = b[1];
        return;
    }

    /* p is the larger in magnitude, so a subtraction cannot go negative */
    p = &x;
    q = &y;
    if (x.exp < y.exp || (x.exp == y.exp && less64(x.sig, y.sig))) {
        p = &y;
        q = &x;
    }
    shr_jam(p->sig, 1);                 /* room for a carry; exact, as the low bits are 0 */
    shr_jam(q->sig, 1 + p->exp - q->exp);
    p->exp++;
    if (p->sign == q->sign)
        add64(p->sig, q->sig);
    else {
        sub64(p->sig, q->sig);
        if ((p->sig[0] | p->sig[1]) == 0) {
            special(r, 0, 0);           /* an exact cancellation gives +0 */
            return;
        }
    }
    pack(r, p);
}

void d_add(uint32_t *r, uint32_t *a, uint32_t *b)
{
    add_signed(r, a, b, 0);
}

void d_sub(uint32_t *r, uint32_t *a, uint32_t *b)
{
    add_signed(r, a, b, 1);
}

void d_mul(uint32_t *r, uint32_t *a, uint32_t *b)
{
    struct unpacked x;
    struct unpacked y;

    unpack(&x, a);
    unpack(&y, b);
    x.sign ^= y.sign;
    if (x.kind == KIND_NAN || y.kind == KIND_NAN) {
        make_nan(r);
        return;
    }
    if (x.kind == KIND_INF || y.kind == KIND_INF) {
        if (x.kind == KIND_ZERO || y.kind == KIND_ZERO)
            make_nan(r);
        else
            special(r, x.sign, INF_HIGH);
        return;
    }
    if (x.kind == KIND_ZERO || y.kind == KIND_ZERO) {
        special(r, x.sign, 0);
        return;
    }
    mul64(x.sig, y.sig);
    x.exp += y.exp + 1;
    pack(r, &x);
}

void d_div(uint32_t *r, uint32_t *a, uint32_t *b)
{
    struct unpacked x;
    struct unpacked y;
    struct unpacked *p;
    uint32_t q[2];

    unpack(&x, a);
    unpack(&y, b);
    x.sign ^= y.sign;
    if (x.kind == KIND_NAN || y.kind == KIND_NAN
            || (x.kind == KIND_INF && y.kind == KIND_INF)
            || (x.kind == KIND_ZERO && y.kind == KIND_ZERO)) {
        make_nan(r);
        return;
    }
    if (x.kind == KIND_INF || y.kind == KIND_ZERO) {
        special(r, x.sign, INF_HIGH);
        return;
    }
    if (x.kind == KIND_ZERO || y.kind == KIND_INF) {
        special(r, x.sign, 0);
        return;
    }

    /* Long division, with the dividend in [divisor, 2 * divisor) */
    shr_jam(x.sig, 1);                  /* room to shift the remainder; exact */
    shr_jam(y.sig, 1);
    x.exp -= y.exp;
    if (less64(x.sig, y.sig)) {
        shl1(x.sig);
        x.exp--;
    }
    div64(x.sig, y.sig, q);
    p = &x;                             /* sccz80 indexes x.sig[i] at the wrong offset */
    if ((p->sig[0] | p->sig[1]) != 0)
        q[1] |= 1;                      /* an inexact quotient */
    p->sig[0] = q[0];
    p->sig[1] = q[1];
    pack(r, p);
}

void d_neg(uint32_t *r, uint32_t *a)
{
    r[0] = a[0] ^ SIGN_BIT;
    r[1] = a[1];
}

void d_from_int(uint32_t *r, int32_t i)
{
    struct unpacked u;
    struct unpacked *p;

    if (i == 0) {
        special(r, 0, 0);
        return;
    }
    p = &u;                             /* sccz80 indexes u.sig[i] at the wrong offset */
    p->sign = i < 0;
    p->sig[0] = p->sign ? ~(uint32_t)i + 1 : (uint32_t)i;
    p->sig[1] = 0;
    p->exp = 31;
    pack(r, p);                         /* exact: an int has at most 32 bits */
}

/* As Java's (int): truncates towards zero, saturates, and NaN becomes 0. */
int32_t d_to_int(uint32_t *a)
{
    struct unpacked u;
    struct unpacked *p;
    uint32_t v;

    p = &u;                             /* sccz80 indexes u.sig[i] at the wrong offset */
    unpack(p, a);
    if (p->kind == KIND_NAN || p->kind == KIND_ZERO)
        return 0;
    if (p->kind == KIND_INF || p->exp >= 31)
        return p->sign ? (int32_t)SIGN_BIT : (int32_t)0x7FFFFFFFL;
    if (p->exp < 0)
        return 0;
    v = p->sig[0] >> (31 - p->exp);
    return p->sign ? (int32_t)(~v + 1) : (int32_t)v;
}

static int is_nan(uint32_t *a)
{
    uint32_t high;

    high = a[0] & ~SIGN_BIT;
    return high > INF_HIGH || (high == INF_HIGH && a[1] != 0);
}

int d_cmp(uint32_t *a, uint32_t *b, int nan_result)
{
    unsigned char sign;
    unsigned char less;

    if (is_nan(a) || is_nan(b))
        return nan_result;
    if ((((a[0] | b[0]) & ~SIGN_BIT) | a[1] | b[1]) == 0)
        return 0;                       /* -0.0 == 0.0 */
    sign = (a[0] & SIGN_BIT) != 0;
    if (sign != ((b[0] & SIGN_BIT) != 0))
        return sign ? -1 : 1;
    if (a[0] == b[0] && a[1] == b[1])
        return 0;
    less = less64(a, b);                /* compares magnitudes, as the signs are equal */
    return less != sign ? -1 : 1;
}
