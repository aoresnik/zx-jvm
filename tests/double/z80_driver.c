/*
 * Runs src/double.c on the Z80 over test vectors that z80_test.py puts in
 * memory, and leaves the results next to them.
 */

#pragma output CRT_ORG_CODE = 32768
#pragma output CRT_ENABLE_STDIO = 0
#pragma output CLIB_EXIT_STACK_SIZE = 0
#pragma output CLIB_MALLOC_HEAP_SIZE = 0
#pragma output CLIB_STDIO_HEAP_SIZE = 0

#include <string.h>

#include "../../src/double.h"

#define OP_ADD  0
#define OP_SUB  1
#define OP_MUL  2
#define OP_DIV  3
#define OP_CMPL 4
#define OP_CMPG 5
#define OP_D2I  6
#define OP_I2D  7
#define OP_NEG  8
#define OP_DEC  9   /* d_to_decimal: the digits in b, the exponent in r */

/* 25 bytes, with each double as two little-endian words, the high word first */
struct vector {
    unsigned char op;
    uint32_t a[2];
    uint32_t b[2];
    uint32_t r[2];
};

#define COUNT   (*(unsigned int *)0xBFFE)
#define VECTORS ((struct vector *)0xC000)

int main(void)
{
    struct vector *v;
    unsigned int n;
    char digits[15];
    int k;

    v = VECTORS;
    for (n = COUNT; n != 0; n--) {
        switch (v->op) {
        case OP_ADD:  d_add(v->r, v->a, v->b); break;
        case OP_SUB:  d_sub(v->r, v->a, v->b); break;
        case OP_MUL:  d_mul(v->r, v->a, v->b); break;
        case OP_DIV:  d_div(v->r, v->a, v->b); break;
        case OP_CMPL: v->r[0] = (int32_t)d_cmp(v->a, v->b, -1); break;
        case OP_CMPG: v->r[0] = (int32_t)d_cmp(v->a, v->b, 1); break;
        case OP_D2I:  v->r[0] = (uint32_t)d_to_int(v->a); break;
        case OP_I2D:  d_from_int(v->r, (int32_t)v->a[0]); break;
        case OP_NEG:  d_neg(v->r, v->a); break;
        case OP_DEC:
            k = d_to_decimal(v->a, digits);
            memcpy(v->b, digits, 8);
            memcpy(v->r, digits + 8, 7);
            ((char *)v->r)[7] = (char)k;    /* the low byte; the Python side knows the rough exponent */
            break;
        }
        v++;
    }
    return 0;
}
