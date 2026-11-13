/*
 * Compares src/double.c, compiled for the host, with the host's IEEE 754
 * hardware on special values and random operands.
 *
 * usage: host_test [ITERATIONS]
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/double.h"

static uint64_t state = 0x9E3779B97F4A7C15ULL;

static uint64_t next64(void)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

static uint64_t bits(double d)
{
    uint64_t u;

    memcpy(&u, &d, 8);
    return u;
}

static double from_bits(uint64_t u)
{
    double d;

    memcpy(&d, &u, 8);
    return d;
}

static void split(uint64_t u, uint32_t *w)
{
    w[0] = (uint32_t)(u >> 32);
    w[1] = (uint32_t)u;
}

static uint64_t join(uint32_t *w)
{
    return ((uint64_t)w[0] << 32) | w[1];
}

static const uint64_t specials[] = {
    0x0000000000000000ULL, 0x8000000000000000ULL,     /* zeros */
    0x0000000000000001ULL, 0x8000000000000001ULL,     /* smallest subnormals */
    0x000FFFFFFFFFFFFFULL, 0x0010000000000000ULL,     /* largest subnormal, smallest normal */
    0x7FEFFFFFFFFFFFFFULL, 0xFFEFFFFFFFFFFFFFULL,     /* largest finite */
    0x7FF0000000000000ULL, 0xFFF0000000000000ULL,     /* infinities */
    0x7FF8000000000000ULL, 0x7FF0000000000001ULL,     /* NaNs */
    0x3FF0000000000000ULL, 0xBFF0000000000000ULL,     /* 1, -1 */
    0x3FF0000000000001ULL, 0x3FEFFFFFFFFFFFFFULL,     /* around 1 */
    0x4340000000000000ULL, 0x41DFFFFFFFC00000ULL,     /* 2^53, 2^31 - 1 */
    0xC1E0000000000000ULL, 0x41E0000000000000ULL,     /* -2^31, 2^31 */
    0x3FB999999999999AULL, 0x3FC999999999999AULL,     /* 0.1, 0.2 */
};

#define SPECIALS (sizeof(specials) / sizeof(specials[0]))

/* Random doubles of several shapes, so that rounding, cancellation and subnormals all occur */
static uint64_t random_double(uint64_t near)
{
    uint64_t r;

    r = next64();
    switch (next64() % 8) {
    case 0:
        return r;                                           /* any bits */
    case 1:
        return specials[next64() % SPECIALS];
    case 2:                                                 /* exponent close to the other operand */
        return (near & 0x7FF0000000000000ULL) + ((next64() % 7) << 52) - (3ULL << 52)
            + (r & 0x800FFFFFFFFFFFFFULL);
    case 3:                                                 /* subnormal or tiny */
        return (r & 0x801FFFFFFFFFFFFFULL);
    case 4:                                                 /* few significant bits, for exact ties */
        return (r & 0xFFFFF00000000000ULL) | (next64() % 2 ? 0 : 0x0000080000000000ULL);
    case 5:                                                 /* near overflow */
        return (r & 0x800FFFFFFFFFFFFFULL) | 0x7FD0000000000000ULL | ((next64() % 3) << 52);
    case 6:                                                 /* a small integer */
        return bits((double)(int32_t)(next64() % 2001) - 1000);
    default:                                                /* ordinary magnitudes */
        return (r & 0x800FFFFFFFFFFFFFULL) | ((0x3F0ULL + next64() % 32) << 52);
    }
}

static int32_t java_d2i(double d)
{
    if (isnan(d))
        return 0;
    if (d >= 2147483647.0)
        return INT32_MAX;
    if (d <= -2147483648.0)
        return INT32_MIN;
    return (int32_t)d;
}

static int java_cmp(double a, double b, int nan_result)
{
    if (isnan(a) || isnan(b))
        return nan_result;
    return a < b ? -1 : a > b ? 1 : 0;
}

static long failures;

static void check(const char *op, uint64_t a, uint64_t b, uint64_t got, uint64_t want)
{
    if (got == want || (isnan(from_bits(got)) && isnan(from_bits(want))))
        return;
    if (failures++ < 20)
        printf("FAIL %s %016llx %016llx: got %016llx, want %016llx\n", op,
               (unsigned long long)a, (unsigned long long)b,
               (unsigned long long)got, (unsigned long long)want);
}

static void test_pair(uint64_t a, uint64_t b)
{
    uint32_t x[2];
    uint32_t y[2];
    uint32_t r[2];
    double da;
    double db;

    da = from_bits(a);
    db = from_bits(b);
    split(a, x);
    split(b, y);
    d_add(r, x, y);
    check("add", a, b, join(r), bits(da + db));
    d_sub(r, x, y);
    check("sub", a, b, join(r), bits(da - db));
    d_mul(r, x, y);
    check("mul", a, b, join(r), bits(da * db));
    d_div(r, x, y);
    check("div", a, b, join(r), bits(da / db));
    d_neg(r, x);
    check("neg", a, 0, join(r), a ^ 0x8000000000000000ULL);
    check("cmpl", a, b, (uint64_t)(int64_t)d_cmp(x, y, -1), (uint64_t)(int64_t)java_cmp(da, db, -1));
    check("cmpg", a, b, (uint64_t)(int64_t)d_cmp(x, y, 1), (uint64_t)(int64_t)java_cmp(da, db, 1));
    check("d2i", a, 0, (uint64_t)(int64_t)d_to_int(x), (uint64_t)(int64_t)java_d2i(da));
}

static long decimals;
static long near_halfway;

/*
 * Compares d_to_decimal with glibc's printf, which rounds exactly. The
 * scaling by 10^k is not exact, so the last digit may go the other way when
 * |a| is within a thousandth of a unit of halfway between two decimals.
 */
static void test_decimal(uint64_t a)
{
    uint32_t x[2];
    char digits[15];
    char want[40];
    char exact[60];
    char got[40];
    int k;
    unsigned int beyond;

    if ((a & 0x7FFFFFFFFFFFFFFFULL) == 0 || (a & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL)
        return;
    decimals++;
    split(a, x);
    k = d_to_decimal(x, digits);
    snprintf(got, sizeof got, "%c.%.14se%+03d", digits[0], digits + 1, k);
    snprintf(want, sizeof want, "%.14e", fabs(from_bits(a)));
    if (strcmp(got, want) == 0)
        return;
    snprintf(exact, sizeof exact, "%.40e", fabs(from_bits(a)));
    beyond = (unsigned int)atoi((char[]){ exact[16], exact[17], exact[18], exact[19], 0 });
    if (beyond >= 4990 && beyond <= 5009) {
        near_halfway++;
        return;
    }
    if (failures++ < 20)
        printf("FAIL decimal %016llx: got %s, want %s\n", (unsigned long long)a, got, want);
}

int main(int argc, char **argv)
{
    long iterations;
    long n;
    unsigned int i;
    unsigned int j;
    uint32_t r[2];
    int32_t v;
    uint64_t a;

    iterations = argc > 1 ? atol(argv[1]) : 1000000;
    for (i = 0; i < SPECIALS; i++)
        for (j = 0; j < SPECIALS; j++)
            test_pair(specials[i], specials[j]);
    for (n = 0; n < iterations; n++) {
        a = random_double(0);
        test_pair(a, random_double(a));
        v = (int32_t)next64();
        if (n % 4 == 0)
            v >>= next64() % 32;
        d_from_int(r, v);
        check("i2d", (uint64_t)(uint32_t)v, 0, join(r), bits((double)v));
        test_decimal(a);
    }
    for (i = 0; i < SPECIALS; i++)
        test_decimal(specials[i]);
    printf("%ld operand pairs, %ld failures\n", iterations + SPECIALS * SPECIALS, failures);
    printf("%ld doubles to decimal, %ld rounded the other way as almost halfway\n",
           decimals, near_halfway);
    return failures != 0;
}
