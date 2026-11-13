/*
 * IEEE 754 binary64 arithmetic in software; see double.h.
 *
 * The algorithms follow Berkeley SoftFloat: operands are unpacked into a sign,
 * an exponent and a 64-bit significand with spare low bits, the operation is
 * done exactly or with the lost bits ORed into bit 0 ("jamming"), and the
 * result is rounded once when it is packed again.
 *
 * The operations are in C, shared by the Z80 and the host, and work on the
 * static unpacked numbers ua, ub and uc. The primitives below them, such as
 * unpacking, packing and arithmetic on significands, are in Z80 code, as
 * sccz80 compiles them large and slow: it has no 64-bit type and calls a
 * library routine for every 32-bit operation. For the host, where the tests
 * compare the results with the hardware, they are in C.
 */

#include <string.h>

#include "double.h"

#define SIGN 0x80               /* the sign of an unpacked number */

/* value = sig * 2^(exp - 63), with bit 63 of sig set when the kind is KIND_FINITE */
struct unpacked {
    unsigned char sig[8];       /* least significant byte first */
    int exp;
    unsigned char sign;         /* 0 or SIGN */
};

static struct unpacked ua;
static struct unpacked ub;
static struct unpacked uc;

/* ---- Primitives ---- */

/*
 * unpack:      u = d, returning its kind; a finite u is normalized
 * pack:        r = u, rounded to nearest, ties to even; u is finite and not zero
 * normalize:   shifts u's non-zero significand left until bit 63 is set
 *
 * On significands:
 * sig_shl1:    a <<= 1
 * sig_shr_jam: a >>= n, keeping any lost bits as bit 0 so that rounding still sees them
 * sig_round11: a = a / 2^11, rounded to nearest, ties to even
 * sig_add:     a += b
 * sig_sub:     a -= b
 * sig_cmp:     -1, 0 or 1 as a < b, a == b or a > b
 * sig_is_zero: a == 0
 * sig_mul:     a = the high 64 bits of a * b, jammed
 * sig_div:     a = the 64-bit quotient a * 2^63 / b, jammed, for b <= a < 2b
 *              with bit 63 of both clear
 */

#ifdef __SCCZ80

/*
 * Z80 routines with register arguments, called by the C functions after
 * them. A packed double is the high word then the low word, each
 * little-endian, so its bytes from the least significant are at offsets 4,
 * 5, 6, 7, 0, 1, 2, 3. An unpacked number is the significand at offset 0,
 * exp at 8 and sign at 10. The loops favour size over speed. IX and IY are
 * kept, IY for the ROM's interrupt routine.
 *
 * A C function with arguments on the stack and no locals has no prologue, so
 * it can jump to a routine, which then returns for it. A fastcall function
 * pushes its argument, HL, on entry, so it must call the routine and let the
 * compiler's code after it return.
 */

static unsigned char *mul_a;
static unsigned char *mul_b;            /* the next byte of the second operand */
static unsigned char byte_bits;         /* the bits of the current byte of the multiplier or quotient */
static unsigned char mul_acc[9];        /* the high half of the product, then whether bits were lost */
static unsigned char div_q[8];
static unsigned char *div_q_next;       /* just above the next byte of the quotient */
static unsigned char *div_n;
static unsigned char *div_d;
static unsigned char *pack_r;
static unsigned char loop_count;

#asm
    SECTION code_compiler

; hl = the first of two pointer arguments of the C function that called this, de = the second
.args2
    ld   hl,4
    add  hl,sp
    ld   e,(hl)
    inc  hl
    ld   d,(hl)
    inc  hl
    ld   a,(hl)
    inc  hl
    ld   h,(hl)
    ld   l,a
    ret

; Copies the packed double at de to the significand at hl
.dload
    ex   de,hl
    push hl
    ld   bc,4
    add  hl,bc
    ldir                ; the low word
    pop  hl
    ld   c,4
    ldir                ; the high word
    ret

; Copies the significand at hl to the packed double at de
.dstore
    push de
    ex   de,hl
    ld   bc,4
    add  hl,bc
    ex   de,hl
    ldir                ; the low word
    pop  de
    ld   c,4
    ldir                ; the high word
    ret

; (hl) <<= 1, with the bit shifted out in the carry
.dsig_shl1
    ld   b,8
    or   a
.shl1_loop
    rl   (hl)
    inc  hl
    djnz shl1_loop
    ret

; (hl) >>= 1, with the bit shifted out in the carry; drr1 shifts the carry in
.dsig_shr1
    or   a
.drr1
    ld   bc,7
    push af
    add  hl,bc
    pop  af
    ld   b,8
.rr1_loop
    rr   (hl)
    dec  hl
    djnz rr1_loop
    ret

; (hl) += (de)
.dsig_add
    ld   b,8
    or   a
.add_loop
    ld   a,(de)
    adc  a,(hl)
    ld   (hl),a
    inc  hl
    inc  de
    djnz add_loop
    ret

; (hl) -= (de)
.dsig_sub
    ex   de,hl
    ld   b,8
    or   a
.sub_loop
    ld   a,(de)
    sbc  a,(hl)
    ld   (de),a
    inc  hl
    inc  de
    djnz sub_loop
    ret

; Compares (hl) with (de): carry if less, zero if equal
.dsig_cmp
    ex   de,hl
    ld   bc,7
    add  hl,bc
    ex   de,hl
    add  hl,bc
    ex   de,hl          ; de = the top of the first, hl of the second
    ld   b,8
.cmp_loop
    ld   a,(de)
    cp   (hl)
    ret  nz
    dec  hl
    dec  de
    djnz cmp_loop
    ret

; Zero flag set if (hl) == 0
.dsig_is_zero
    ld   b,8
    xor  a
.zero_loop
    or   (hl)
    inc  hl
    djnz zero_loop
    ret

; (hl) >>= de, jammed
.dshr_jam
    ld   c,0            ; the bits lost, ORed
    ld   a,d
    or   a
    jr   nz,shrj_all
    ld   a,e
    cp   64
    jr   nc,shrj_all
.shrj_bytes             ; whole bytes first
    cp   8
    jr   c,shrj_bits
    sub  8
    push af
    ld   a,c
    or   (hl)
    push af
    push hl
    ld   d,h
    ld   e,l
    inc  hl
    ld   bc,7
    ldir
    xor  a
    ld   (de),a
    pop  hl
    pop  af
    ld   c,a
    pop  af
    jr   shrj_bytes
.shrj_bits
    or   a
    jr   z,shrj_sticky
    ld   b,a
.shrj_bit
    push bc
    push hl
    call dsig_shr1
    pop  hl
    pop  bc
    jr   nc,shrj_kept
    ld   c,1
.shrj_kept
    djnz shrj_bit
    jr   shrj_sticky
.shrj_all
    ld   b,8
    push hl
.shrj_clear
    ld   a,c
    or   (hl)
    ld   c,a
    ld   (hl),0
    inc  hl
    djnz shrj_clear
    pop  hl
.shrj_sticky
    ld   a,c
    or   a
    ret  z
    set  0,(hl)
    ret

; (hl) = (hl) / 2^11, rounded to nearest, ties to even
.dround11
    push hl
    inc  hl
    ld   a,(hl)
    dec  hl
    bit  2,a            ; bit 10, worth half of bit 11
    jr   z,r11_down
    and  3
    or   (hl)           ; bits 0 to 9: above half
    jr   nz,r11_up
    inc  hl
    bit  3,(hl)         ; exactly half: to even
    dec  hl
    jr   z,r11_down
.r11_up                 ; add 2^11
    inc  hl
    ld   a,(hl)
    add  a,8
    ld   (hl),a
    ld   b,6
.r11_carry
    inc  hl
    ld   a,(hl)
    adc  a,0
    ld   (hl),a
    djnz r11_carry
    jr   r11_shift      ; with the carry out of bit 63
.r11_down
    or   a
.r11_shift              ; shift the carry and (hl) right 3 bits, then 8
    pop  hl
    push hl
    call drr1
    pop  hl
    push hl
    call dsig_shr1
    pop  hl
    push hl
    call dsig_shr1
    pop  hl
    ld   d,h
    ld   e,l
    inc  hl
    ld   bc,7
    ldir
    xor  a
    ld   (de),a
    ret

; Adds a, sign-extended, to the exp of the unpacked number at hl, keeping hl
.add_exp
    push hl
    ld   bc,8
    add  hl,bc
    ld   c,a
    rla
    sbc  a,a
    ld   b,a
    ld   a,(hl)
    add  a,c
    ld   (hl),a
    inc  hl
    ld   a,(hl)
    adc  a,b
    ld   (hl),a
    pop  hl
    ret

; Normalizes the unpacked number at hl, which is not zero
.dnormalize
    push hl
    ld   bc,7
    add  hl,bc
    ld   a,(hl)
    pop  hl
    or   a
    jr   nz,norm_bit
    push hl             ; shift left a byte
    ld   bc,6
    add  hl,bc
    ld   d,h
    ld   e,l
    inc  de
    ld   bc,7
    lddr
    xor  a
    ld   (de),a
    pop  hl
    ld   a,-8
    call add_exp
    jr   dnormalize
.norm_bit
    rla
    ret  c              ; bit 63 is set
    push hl
    call dsig_shl1
    pop  hl
    ld   a,-1
    call add_exp
    jr   dnormalize

; Unpacks the double at de into the unpacked number at hl; hl = its kind
.dunpack
    push ix
    push hl
    pop  ix
    call dload
    ld   a,(ix+7)
    and  $80
    ld   (ix+10),a      ; the sign
    xor  (ix+7)
    ld   h,a
    ld   l,(ix+6)
    ld   (ix+7),0
    ld   a,l
    and  $0F
    ld   (ix+6),a       ; the 52-bit fraction
    srl  h
    rr   l
    srl  h
    rr   l
    srl  h
    rr   l
    srl  h
    rr   l              ; hl = the biased exponent
    push hl
    push ix
    pop  hl
    call dsig_is_zero
    pop  hl
    ld   c,2            ; KIND_INF for a zero fraction
    jr   z,unp_kind
    inc  c              ; KIND_NAN
.unp_kind
    ld   a,h
    cp   7
    jr   nz,unp_finite
    ld   a,l
    inc  a
    jr   z,unp_done     ; the exponent is all ones
.unp_finite
    ld   a,h
    or   l
    jr   nz,unp_normal
    ld   a,c
    cp   2
    ld   c,0            ; KIND_ZERO
    jr   z,unp_done
    inc  l              ; subnormal: as the smallest exponent, without the implicit bit
    jr   unp_exp
.unp_normal
    set  4,(ix+6)       ; the implicit bit
.unp_exp
    ld   bc,-1012       ; normalizing shifts a normal number 11 bits
    add  hl,bc
    ld   (ix+8),l
    ld   (ix+9),h
    push ix
    pop  hl
    call dnormalize
    ld   c,1            ; KIND_FINITE
.unp_done
    ld   l,c
    ld   h,0
    pop  ix
    ret

; Packs the unpacked number at de into the double at hl
.dpack
    push ix
    ld   (_pack_r),hl
    push de
    pop  ix
    ex   de,hl
    call dnormalize
    ld   l,(ix+8)
    ld   h,(ix+9)
    ld   bc,1023
    add  hl,bc          ; hl = the biased exponent, if the result is normal
    bit  7,h
    jr   nz,pack_tiny
    ld   a,h
    or   l
    jr   nz,pack_round
.pack_tiny              ; subnormal: align to the smallest exponent
    ex   de,hl
    ld   hl,1
    or   a
    sbc  hl,de
    ex   de,hl
    push ix
    pop  hl
    call dshr_jam
    ld   hl,1
.pack_round
    push hl
    push ix
    pop  hl
    call dround11
    pop  hl
    bit  5,(ix+6)
    jr   z,pack_check
    ld   (ix+6),$10     ; rounded up to the next power of two
    inc  hl
.pack_check
    ld   de,$7FF
    or   a
    sbc  hl,de
    add  hl,de          ; carry if hl < $7FF
    jr   nc,pack_inf
    bit  4,(ix+6)
    jr   nz,pack_bits
    ld   hl,0           ; subnormal
.pack_bits
    ld   a,l
    add  a,a
    add  a,a
    add  a,a
    add  a,a
    ld   b,a
    ld   a,(ix+6)
    and  $0F
    or   b
    ld   (ix+6),a
    srl  h
    rr   l
    srl  h
    rr   l
    srl  h
    rr   l
    srl  h
    rr   l
    ld   a,l
    or   (ix+10)
    ld   (ix+7),a
    jr   pack_store
.pack_inf
    push ix
    pop  hl
    ld   b,6
.pack_clear
    ld   (hl),0
    inc  hl
    djnz pack_clear
    ld   (ix+6),$F0
    ld   a,(ix+10)
    or   $7F
    ld   (ix+7),a
.pack_store
    push ix
    pop  hl
    ld   de,(_pack_r)
    pop  ix
    jp   dstore

; (hl) = the high 64 bits of (hl) * (de), jammed. Shift and add: for each bit
; of the second from the lowest, add the first to the high half of the product
; if the bit is set, then shift the product right. A zero byte shifts it 8 bits.
.dsig_mul
    ld   (_mul_a),hl
    ld   (_mul_b),de
    ld   hl,_mul_acc
    ld   b,9
.mul_clear
    ld   (hl),0
    inc  hl
    djnz mul_clear
    ld   a,8
    ld   (_loop_count),a
.mul_byte
    ld   hl,(_mul_b)
    ld   a,(hl)
    inc  hl
    ld   (_mul_b),hl
    or   a
    jr   nz,mul_bits
    ld   hl,_mul_acc
    or   (hl)
    jr   z,mul_byte_kept
    ld   a,1
    ld   (_mul_acc+8),a
.mul_byte_kept
    ld   d,h
    ld   e,l
    inc  hl
    ld   bc,7
    ldir
    xor  a
    ld   (de),a
    jr   mul_next_byte
.mul_bits
    ld   (_byte_bits),a
    ld   b,8
.mul_bit
    push bc
    ld   hl,_byte_bits
    srl  (hl)
    jr   nc,mul_shift   ; with the carry clear
    ld   hl,_mul_acc
    ld   de,(_mul_a)
    call dsig_add
.mul_shift
    ld   hl,_mul_acc
    call drr1
    jr   nc,mul_kept
    ld   a,1
    ld   (_mul_acc+8),a
.mul_kept
    pop  bc
    djnz mul_bit
.mul_next_byte
    ld   hl,_loop_count
    dec  (hl)
    jr   nz,mul_byte
    ld   hl,_mul_acc
    ld   de,(_mul_a)
    ld   bc,8
    ldir
    ld   a,(_mul_acc+8)
    or   a
    ret  z
    ld   hl,(_mul_a)
    set  0,(hl)
    ret

; (hl) = (hl) * 2^63 / (de), jammed. Restoring division, a quotient bit at a
; time from the highest: if n >= d, subtract d and the bit is 1; then shift n
; left. The bits are collected a byte at a time.
.dsig_div
    ld   (_div_n),hl
    ld   (_div_d),de
    ld   hl,_div_q+8
    ld   (_div_q_next),hl
    ld   a,8
    ld   (_loop_count),a
.div_byte
    ld   b,8
.div_bit
    push bc
    ld   hl,(_div_n)
    ld   de,(_div_d)
    call dsig_cmp
    jr   c,div_shift    ; n < d: with the carry, which is inverted below
    ld   hl,(_div_n)
    ld   de,(_div_d)
    call dsig_sub       ; leaves the carry clear
.div_shift
    ccf
    ld   hl,_byte_bits
    rl   (hl)
    ld   hl,(_div_n)
    call dsig_shl1
    pop  bc
    djnz div_bit
    ld   hl,(_div_q_next)
    dec  hl
    ld   a,(_byte_bits)
    ld   (hl),a
    ld   (_div_q_next),hl
    ld   hl,_loop_count
    dec  (hl)
    jr   nz,div_byte
    ld   hl,(_div_n)
    call dsig_is_zero
    jr   z,div_exact
    ld   hl,_div_q
    set  0,(hl)         ; an inexact quotient
.div_exact
    ld   hl,_div_q
    ld   de,(_div_n)
    ld   bc,8
    ldir
    ret
#endasm

static unsigned char unpack(struct unpacked *u, uint32_t *d)
{
#asm
    call args2
    jp   dunpack
#endasm
}

static void pack(uint32_t *r, struct unpacked *u)
{
#asm
    call args2
    jp   dpack
#endasm
}

static void normalize(struct unpacked *u) __z88dk_fastcall
{
#asm
    call dnormalize
#endasm
}

static void sig_shl1(unsigned char *a) __z88dk_fastcall
{
#asm
    call dsig_shl1
#endasm
}

static void sig_shr_jam(unsigned char *a, int n)
{
#asm
    call args2
    jp   dshr_jam
#endasm
}

static void sig_round11(unsigned char *a) __z88dk_fastcall
{
#asm
    call dround11
#endasm
}

static void sig_add(unsigned char *a, unsigned char *b)
{
#asm
    call args2
    jp   dsig_add
#endasm
}

static void sig_sub(unsigned char *a, unsigned char *b)
{
#asm
    call args2
    jp   dsig_sub
#endasm
}

static int sig_cmp(unsigned char *a, unsigned char *b)
{
#asm
    call args2
    call dsig_cmp
    ld   hl,0
    ret  z
    dec  hl
    ret  c
    ld   hl,1
#endasm
}

static int sig_is_zero(unsigned char *a) __z88dk_fastcall
{
#asm
    call dsig_is_zero
    ld   hl,0
    jr   nz,is_zero_done
    inc  l
.is_zero_done
#endasm
}

static void sig_mul(unsigned char *a, unsigned char *b)
{
#asm
    call args2
    jp   dsig_mul
#endasm
}

static void sig_div(unsigned char *a, unsigned char *b)
{
#asm
    call args2
    jp   dsig_div
#endasm
}

#else

static uint64_t get64(unsigned char *s)
{
    uint64_t v;
    int i;

    v = 0;
    for (i = 7; i >= 0; i--)
        v = (v << 8) | s[i];
    return v;
}

static void put64(unsigned char *s, uint64_t v)
{
    int i;

    for (i = 0; i < 8; i++, v >>= 8)
        s[i] = (unsigned char)v;
}

static void normalize(struct unpacked *u)
{
    uint64_t v;

    v = get64(u->sig);
    while ((v >> 63) == 0) {
        v <<= 1;
        u->exp--;
    }
    put64(u->sig, v);
}

static unsigned char unpack(struct unpacked *u, uint32_t *d)
{
    uint64_t m;
    unsigned int e;

    u->sign = (d[0] >> 31) != 0 ? SIGN : 0;
    e = (d[0] >> 20) & 0x7FF;
    m = ((uint64_t)(d[0] & 0xFFFFF) << 32) | d[1];
    if (e == 0x7FF)
        return m != 0 ? KIND_NAN : KIND_INF;
    if (e == 0) {
        if (m == 0)
            return KIND_ZERO;
        e = 1;                          /* subnormal: no implicit bit */
    } else {
        m |= 1ULL << 52;
    }
    put64(u->sig, m << 11);
    u->exp = (int)e - 1023;
    normalize(u);
    return KIND_FINITE;
}

static void sig_shl1(unsigned char *a)
{
    put64(a, get64(a) << 1);
}

static void sig_shr_jam(unsigned char *a, int n)
{
    uint64_t v;

    v = get64(a);
    if (n >= 64)
        v = v != 0;
    else if (n > 0)
        v = (v >> n) | ((v & ((1ULL << n) - 1)) != 0);
    put64(a, v);
}

static void sig_round11(unsigned char *a)
{
    uint64_t v;
    unsigned int below;

    v = get64(a);
    below = v & 0x7FF;
    v >>= 11;
    if (below > 0x400 || (below == 0x400 && (v & 1) != 0))
        v++;
    put64(a, v);
}

static void pack(uint32_t *r, struct unpacked *u)
{
    uint64_t m;
    int e;

    normalize(u);
    e = u->exp + 1023;
    if (e < 1) {
        sig_shr_jam(u->sig, 1 - e);
        e = 1;
    }
    sig_round11(u->sig);
    m = get64(u->sig);
    if (m == 1ULL << 53) {              /* rounded up to the next power of two */
        m = 1ULL << 52;
        e++;
    }
    if (e >= 0x7FF) {
        m = 0x7FF0000000000000ULL;
    } else {
        if ((m >> 52) == 0)
            e = 0;                      /* subnormal */
        m = (m & 0xFFFFFFFFFFFFFULL) | ((uint64_t)e << 52);
    }
    if (u->sign != 0)
        m |= 1ULL << 63;
    r[0] = (uint32_t)(m >> 32);
    r[1] = (uint32_t)m;
}

static void sig_add(unsigned char *a, unsigned char *b)
{
    put64(a, get64(a) + get64(b));
}

static void sig_sub(unsigned char *a, unsigned char *b)
{
    put64(a, get64(a) - get64(b));
}

static int sig_cmp(unsigned char *a, unsigned char *b)
{
    uint64_t x;
    uint64_t y;

    x = get64(a);
    y = get64(b);
    return x < y ? -1 : x > y;
}

static int sig_is_zero(unsigned char *a)
{
    return get64(a) == 0;
}

static void sig_mul(unsigned char *a, unsigned char *b)
{
    unsigned __int128 p;

    p = (unsigned __int128)get64(a) * get64(b);
    put64(a, (uint64_t)(p >> 64) | ((uint64_t)p != 0));
}

static void sig_div(unsigned char *a, unsigned char *b)
{
    unsigned __int128 n;
    uint64_t d;

    n = (unsigned __int128)get64(a) << 63;
    d = get64(b);
    put64(a, (uint64_t)(n / d) | (n % d != 0));
}

#endif

/*
 * ---- Operations ----
 *
 * Locals are static, as sccz80 reaches statics with shorter code; none of
 * these functions is reentrant.
 */

/* Zero or, if inf, infinity, with a sign */
static void special(uint32_t *r, unsigned char sign, unsigned char inf)
{
    r[0] = inf ? 0x7FF00000UL : 0;
    r[1] = 0;
    if (sign)
        r[0] |= 0x80000000UL;
}

static void make_nan(uint32_t *r)
{
    r[0] = 0x7FF80000UL;
    r[1] = 0;
}

/* u *= the number with significand sig and exponent exp */
static void multiply(struct unpacked *u, unsigned char *sig, int exp)
{
    sig_mul(u->sig, sig);
    u->exp += exp + 1;
}

/* u /= v, by long division with the dividend in [divisor, 2 * divisor); changes v */
static void divide(struct unpacked *u, struct unpacked *v)
{
    sig_shr_jam(u->sig, 1);             /* room to shift the remainder; exact */
    sig_shr_jam(v->sig, 1);
    u->exp -= v->exp;
    if (sig_cmp(u->sig, v->sig) < 0) {
        sig_shl1(u->sig);
        u->exp--;
    }
    sig_div(u->sig, v->sig);
}

static void add_signed(uint32_t *r, uint32_t *a, uint32_t *b, unsigned char negate_b)
{
    static struct unpacked *p;
    static struct unpacked *q;
    static unsigned char ka;
    static unsigned char kb;

    ka = unpack(&ua, a);
    kb = unpack(&ub, b);
    ub.sign ^= negate_b;
    if (ka == KIND_NAN || kb == KIND_NAN) {
        make_nan(r);
        return;
    }
    if (ka == KIND_INF) {
        if (kb == KIND_INF && ua.sign != ub.sign)
            make_nan(r);
        else
            special(r, ua.sign, 1);
        return;
    }
    if (kb == KIND_INF) {
        special(r, ub.sign, 1);
        return;
    }
    if (kb == KIND_ZERO) {
        if (ka == KIND_ZERO)
            special(r, ua.sign & ub.sign, 0);
        else
            pack(r, &ua);               /* a, as packing it again is exact */
        return;
    }
    if (ka == KIND_ZERO) {
        pack(r, &ub);
        return;
    }

    /* p is the larger in magnitude, so a subtraction cannot go negative */
    p = &ua;
    q = &ub;
    if (ua.exp < ub.exp || (ua.exp == ub.exp && sig_cmp(ua.sig, ub.sig) < 0)) {
        p = &ub;
        q = &ua;
    }
    sig_shr_jam(p->sig, 1);             /* room for a carry; exact, as the low bits are 0 */
    sig_shr_jam(q->sig, 1 + p->exp - q->exp);
    p->exp++;
    if (p->sign == q->sign)
        sig_add(p->sig, q->sig);
    else {
        sig_sub(p->sig, q->sig);
        if (sig_is_zero(p->sig)) {
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
    add_signed(r, a, b, SIGN);
}

void d_mul(uint32_t *r, uint32_t *a, uint32_t *b)
{
    static unsigned char ka;
    static unsigned char kb;

    ka = unpack(&ua, a);
    kb = unpack(&ub, b);
    ua.sign ^= ub.sign;
    if (ka == KIND_NAN || kb == KIND_NAN) {
        make_nan(r);
        return;
    }
    if (ka == KIND_INF || kb == KIND_INF) {
        if (ka == KIND_ZERO || kb == KIND_ZERO)
            make_nan(r);
        else
            special(r, ua.sign, 1);
        return;
    }
    if (ka == KIND_ZERO || kb == KIND_ZERO) {
        special(r, ua.sign, 0);
        return;
    }
    multiply(&ua, ub.sig, ub.exp);
    pack(r, &ua);
}

void d_div(uint32_t *r, uint32_t *a, uint32_t *b)
{
    static unsigned char ka;
    static unsigned char kb;

    ka = unpack(&ua, a);
    kb = unpack(&ub, b);
    ua.sign ^= ub.sign;
    if (ka == KIND_NAN || kb == KIND_NAN
            || (ka == KIND_INF && kb == KIND_INF)
            || (ka == KIND_ZERO && kb == KIND_ZERO)) {
        make_nan(r);
        return;
    }
    if (ka == KIND_INF || kb == KIND_ZERO) {
        special(r, ua.sign, 1);
        return;
    }
    if (ka == KIND_ZERO || kb == KIND_INF) {
        special(r, ua.sign, 0);
        return;
    }
    divide(&ua, &ub);
    pack(r, &ua);
}

void d_neg(uint32_t *r, uint32_t *a)
{
    r[0] = a[0] ^ 0x80000000UL;
    r[1] = a[1];
}

void d_from_int(uint32_t *r, int32_t i)
{
    static uint32_t v;

    if (i == 0) {
        special(r, 0, 0);
        return;
    }
    ua.sign = i < 0 ? SIGN : 0;
    v = i < 0 ? ~(uint32_t)i + 1 : (uint32_t)i;
    memset(ua.sig, 0, 4);
    memcpy(ua.sig + 4, &v, 4);          /* little-endian, on the Z80 and the host */
    ua.exp = 31;
    pack(r, &ua);                       /* exact: an int has at most 32 bits */
}

/* As Java's (int): truncates towards zero, saturates, and NaN becomes 0. */
int32_t d_to_int(uint32_t *a)
{
    static unsigned char kind;
    static uint32_t v;

    kind = unpack(&ua, a);
    if (kind == KIND_NAN || kind == KIND_ZERO)
        return 0;
    if (kind == KIND_INF || ua.exp >= 31)
        return ua.sign ? (int32_t)0x80000000UL : (int32_t)0x7FFFFFFFL;
    if (ua.exp < 0)
        return 0;
    memcpy(&v, ua.sig + 4, 4);
    v >>= 31 - ua.exp;
    return ua.sign ? (int32_t)(~v + 1) : (int32_t)v;
}

int d_cmp(uint32_t *a, uint32_t *b, int nan_result)
{
    static unsigned char ka;
    static unsigned char kb;
    static unsigned char less;

    ka = unpack(&ua, a);
    kb = unpack(&ub, b);
    if (ka == KIND_NAN || kb == KIND_NAN)
        return nan_result;
    if (ka == KIND_ZERO && kb == KIND_ZERO)
        return 0;                       /* -0.0 == 0.0 */
    if (ua.sign != ub.sign)
        return ua.sign ? -1 : 1;
    /* The same sign: compare magnitudes, as zero < finite < infinity */
    if (ka != kb)
        less = ka < kb;
    else if (ka != KIND_FINITE)
        return 0;
    else if (ua.exp != ub.exp)
        less = ua.exp < ub.exp;
    else if (sig_cmp(ua.sig, ub.sig) == 0)
        return 0;
    else
        less = sig_cmp(ua.sig, ub.sig) < 0;
    return less != (ua.sign != 0) ? -1 : 1;
}

int d_kind(uint32_t *a)
{
    return unpack(&ua, a);
}

/* ---- Decimal digits ---- */

/* 10^(2^i) = sig * 2^(exp - 63), the significand rounded from 10^32 up */
static int pow10_exp[9] = { 3, 6, 13, 26, 53, 106, 212, 425, 850 };
static unsigned char pow10_sig[72] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0,     /* 10^1 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC8,     /* 10^2 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x9C,     /* 10^4 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0xBC, 0xBE,     /* 10^8 */
    0x00, 0x00, 0x00, 0x04, 0xBF, 0xC9, 0x1B, 0x8E,     /* 10^16 */
    0x9E, 0xB5, 0x70, 0x2B, 0xA8, 0xAD, 0xC5, 0x9D,     /* 10^32 */
    0xD5, 0xA6, 0xCF, 0xFF, 0x49, 0x1F, 0x78, 0xC2,     /* 10^64 */
    0xE0, 0x8C, 0xE9, 0x80, 0xC9, 0x47, 0xBA, 0x93,     /* 10^128 */
    0x8E, 0xDE, 0xF9, 0x9D, 0xFB, 0xEB, 0x7E, 0xAA      /* 10^256 */
};

/* 10^15 down to 10^1 as 64-bit integers */
static unsigned char pow10_int[120] = {
    0x00, 0x80, 0xC6, 0xA4, 0x7E, 0x8D, 0x03, 0x00,     /* 10^15 */
    0x00, 0x40, 0x7A, 0x10, 0xF3, 0x5A, 0x00, 0x00,
    0x00, 0xA0, 0x72, 0x4E, 0x18, 0x09, 0x00, 0x00,
    0x00, 0x10, 0xA5, 0xD4, 0xE8, 0x00, 0x00, 0x00,
    0x00, 0xE8, 0x76, 0x48, 0x17, 0x00, 0x00, 0x00,
    0x00, 0xE4, 0x0B, 0x54, 0x02, 0x00, 0x00, 0x00,     /* 10^10 */
    0x00, 0xCA, 0x9A, 0x3B, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xE1, 0xF5, 0x05, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x96, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x40, 0x42, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xA0, 0x86, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,     /* 10^5 */
    0x10, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xE8, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      /* 10^1 */
};

/* 10^15 = sig * 2^(49 - 63) and 10^14 = sig * 2^(46 - 63) */
static unsigned char e15_sig[8] = { 0x00, 0x00, 0x00, 0xA0, 0x31, 0xA9, 0x5F, 0xE3 };
static unsigned char e14_sig[8] = { 0x00, 0x00, 0x00, 0x80, 0xF4, 0x20, 0xE6, 0xB5 };

/*
 * ub = |ua| * 10^p, normalized, with uc as 10^|p|. 10^p is exact up to 10^27;
 * beyond that, and in the product or quotient, each step is jammed to 64
 * bits, so the result is within a few parts in 2^64.
 */
static void scale10(int p)
{
    static unsigned char i;
    static unsigned char negative;

    negative = p < 0;
    if (negative)
        p = -p;
    memset(uc.sig, 0, 7);               /* 1 */
    uc.sig[7] = 0x80;
    uc.exp = 0;
    for (i = 0; p != 0; i++, p >>= 1) {
        if (p & 1) {
            multiply(&uc, pow10_sig + 8 * i, pow10_exp[i]);
            normalize(&uc);
        }
    }
    memcpy(&ub, &ua, sizeof ub);
    if (negative)
        divide(&ub, &uc);
    else
        multiply(&ub, uc.sig, uc.exp);
    normalize(&ub);
}

static int too_big(void)                /* ub >= 10^15 */
{
    return ub.exp > 49 || (ub.exp == 49 && sig_cmp(ub.sig, e15_sig) >= 0);
}

static int too_small(void)              /* ub < 10^14 */
{
    return ub.exp < 46 || (ub.exp == 46 && sig_cmp(ub.sig, e14_sig) < 0);
}

int d_to_decimal(uint32_t *a, char *digits)
{
    static unsigned char *n;
    static int k;
    static unsigned char i;
    static char digit;

    unpack(&ua, a);

    /* k is about log10(2^exp), with log10(2) about 1233 / 4096, and then corrected */
    if (ua.exp >= 0)
        k = (int)((long)ua.exp * 1233L / 4096L);
    else
        k = -(int)(((long)-ua.exp * 1233L + 4095L) / 4096L);
    scale10(14 - k);
    while (too_big()) {
        k++;
        scale10(14 - k);
    }
    while (too_small()) {
        k--;
        scale10(14 - k);
    }

    /* Round ub, in [10^14, 10^15), to an integer, ties to even */
    n = ub.sig;
    sig_shr_jam(n, 52 - ub.exp);        /* 3 to 6, leaving the fraction in the low 11 bits */
    sig_round11(n);
    if (sig_cmp(n, pow10_int) >= 0) {   /* rounded up to 10^15 */
        memcpy(n, pow10_int + 8, 8);
        k++;
    }

    for (i = 0; i < 14; i++) {
        digit = '0';
        while (sig_cmp(n, pow10_int + 8 * i + 8) >= 0) {
            sig_sub(n, pow10_int + 8 * i + 8);
            digit++;
        }
        digits[i] = digit;
    }
    digits[14] = '0' + n[0];
    return k;
}
