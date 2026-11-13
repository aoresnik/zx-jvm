/*
 * IEEE 754 binary64 arithmetic, as Java's double needs it: round to nearest,
 * ties to even, with subnormals, infinities and NaN.
 *
 * A double is two 32-bit words, the high word first, as it occupies two
 * slots on the JVM's stack. The result pointer may equal an operand pointer.
 * Also compiles for the host, where the tests compare it with the hardware.
 */

#ifndef DOUBLE_H
#define DOUBLE_H

#include <stdint.h>

/* What a double is, as d_kind returns it */
#define KIND_ZERO   0
#define KIND_FINITE 1
#define KIND_INF    2
#define KIND_NAN    3

void d_add(uint32_t *r, uint32_t *a, uint32_t *b);
void d_sub(uint32_t *r, uint32_t *a, uint32_t *b);
void d_mul(uint32_t *r, uint32_t *a, uint32_t *b);
void d_div(uint32_t *r, uint32_t *a, uint32_t *b);
void d_neg(uint32_t *r, uint32_t *a);

void d_from_int(uint32_t *r, int32_t i);
int32_t d_to_int(uint32_t *a);

/* -1, 0 or 1, or nan_result if either is NaN, as Java's dcmpl and dcmpg */
int d_cmp(uint32_t *a, uint32_t *b, int nan_result);

int d_kind(uint32_t *a);

/*
 * Writes the first 15 significant digits of |a|, rounded to nearest, as the
 * characters '0' to '9' and returns the decimal exponent k, so that |a| is
 * about d.dddddddddddddd * 10^k. a must be finite and not zero.
 */
int d_to_decimal(uint32_t *a, char *digits);

#endif
