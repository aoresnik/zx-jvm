/*
 * zxjvm - a very small JVM for the ZX Spectrum.
 *
 * Runs public static void main(String[]) of a single .class file that has
 * already been loaded into memory at CLASS_ADDR. Bytecode is interpreted
 * directly from the class file image; there is no verifier, no garbage
 * collector and no class loading. Runtime classes and methods are implemented
 * in C, listed in classes[], fields[] and natives[].
 *
 * A reference is the 16-bit address of an object, held in a 32-bit slot, and
 * null is 0. A double takes two slots, the high word first, and its
 * arithmetic is in double.c. A String has the layout of a CONSTANT_Utf8
 * entry: constant strings are their entries in the class file, and strings
 * made at run time are allocated on the heap. The first byte of an object is
 * TAG_UTF8 for a String or an OBJ_* kind otherwise.
 *
 * Memory map: this program at JVM_ADDR in the Makefile, 49152 the class
 * file, then the heap up to the user-defined graphics.
 */

/* CRT_ORG_CODE, the address of this program, is set by the Makefile */
#pragma output CRT_ENABLE_STDIO = 0
#pragma output CLIB_EXIT_STACK_SIZE = 0
#pragma output CLIB_MALLOC_HEAP_SIZE = 0
#pragma output CLIB_STDIO_HEAP_SIZE = 0
/* Output goes through the ROM, so leave out z88dk's console driver and its font */
#pragma redirect fputc_cons_generic = _no_console

#include <stdlib.h>
#include <string.h>

#include "double.h"

#define CLASS_ADDR ((unsigned char *)49152)
#define HEAP_END   ((unsigned char *)65368)     /* the user-defined graphics */

/* System variables and ROM data */
#define WORKSP     ((unsigned int *)23649)
#define STKBOT     ((unsigned int *)23651)
#define TKN_TABLE  ((unsigned char *)0x0095)    /* keywords, last letter + 0x80 */

#define MAX_CP     256  /* constant pool entries */
#define MAX_FRAME  64   /* locals + operand stack, in 32-bit slots */

#define ACC_STATIC 0x0008

#define TAG_UTF8    1
#define TAG_INTEGER 3
#define TAG_DOUBLE  6
#define TAG_STRING  8

#define OBJ_PRINTSTREAM 0x80
#define OBJ_INPUTSTREAM 0x81
#define OBJ_SCANNER     0x82

#define REF(p)   ((jint)(unsigned int)(p))
#define DEREF(r) ((unsigned char *)(unsigned int)(r))
#define D(slot)  ((uint32_t *)(slot))  /* the double in two slots */

#define OP_ACONST_NULL   0x01
#define OP_ICONST_M1     0x02
#define OP_ICONST_0      0x03
#define OP_ICONST_1      0x04
#define OP_ICONST_2      0x05
#define OP_ICONST_3      0x06
#define OP_ICONST_4      0x07
#define OP_ICONST_5      0x08
#define OP_DCONST_0      0x0E
#define OP_DCONST_1      0x0F
#define OP_BIPUSH        0x10
#define OP_SIPUSH        0x11
#define OP_LDC           0x12
#define OP_LDC2_W        0x14
#define OP_ILOAD         0x15
#define OP_DLOAD         0x18
#define OP_ALOAD         0x19
#define OP_ILOAD_0       0x1A
#define OP_ILOAD_1       0x1B
#define OP_ILOAD_2       0x1C
#define OP_ILOAD_3       0x1D
#define OP_DLOAD_0       0x26
#define OP_DLOAD_1       0x27
#define OP_DLOAD_2       0x28
#define OP_DLOAD_3       0x29
#define OP_ALOAD_0       0x2A
#define OP_ALOAD_1       0x2B
#define OP_ALOAD_2       0x2C
#define OP_ALOAD_3       0x2D
#define OP_ISTORE        0x36
#define OP_DSTORE        0x39
#define OP_ASTORE        0x3A
#define OP_ISTORE_0      0x3B
#define OP_ISTORE_1      0x3C
#define OP_ISTORE_2      0x3D
#define OP_ISTORE_3      0x3E
#define OP_DSTORE_0      0x47
#define OP_DSTORE_1      0x48
#define OP_DSTORE_2      0x49
#define OP_DSTORE_3      0x4A
#define OP_ASTORE_0      0x4B
#define OP_ASTORE_1      0x4C
#define OP_ASTORE_2      0x4D
#define OP_ASTORE_3      0x4E
#define OP_POP           0x57
#define OP_POP2          0x58
#define OP_DUP           0x59
#define OP_DUP2          0x5C
#define OP_IADD          0x60
#define OP_DADD          0x63
#define OP_ISUB          0x64
#define OP_DSUB          0x67
#define OP_IMUL          0x68
#define OP_DMUL          0x6B
#define OP_IDIV          0x6C
#define OP_DDIV          0x6F
#define OP_IREM          0x70
#define OP_INEG          0x74
#define OP_DNEG          0x77
#define OP_IINC          0x84
#define OP_I2D           0x87
#define OP_D2I           0x8E
#define OP_DCMPL         0x97
#define OP_DCMPG         0x98
#define OP_IFEQ          0x99
#define OP_IFNE          0x9A
#define OP_IFLT          0x9B
#define OP_IFGE          0x9C
#define OP_IFGT          0x9D
#define OP_IFLE          0x9E
#define OP_IF_ICMPEQ     0x9F
#define OP_IF_ICMPNE     0xA0
#define OP_IF_ICMPLT     0xA1
#define OP_IF_ICMPGE     0xA2
#define OP_IF_ICMPGT     0xA3
#define OP_IF_ICMPLE     0xA4
#define OP_IF_ACMPEQ     0xA5
#define OP_IF_ACMPNE     0xA6
#define OP_GOTO          0xA7
#define OP_RETURN        0xB1
#define OP_GETSTATIC     0xB2
#define OP_INVOKEVIRTUAL 0xB6
#define OP_INVOKESPECIAL 0xB7
#define OP_INVOKESTATIC  0xB8
#define OP_NEW           0xBB
#define OP_IFNULL        0xC6
#define OP_IFNONNULL     0xC7

typedef long jint;      /* sccz80: int is 16 bits, long is 32 bits */

static unsigned char *cp[MAX_CP];
static unsigned int cp_count;
static unsigned char resolved[MAX_CP];  /* Methodref -> natives[] index + 1 */
static jint frame[MAX_FRAME];

static unsigned char *heap_top;

static unsigned char system_out[] = { OBJ_PRINTSTREAM };
static unsigned char system_in[] = { OBJ_INPUTSTREAM };

/* Input read by Scanner: the current line, or 0 when a new one is needed */
static unsigned char *scan_line;
static unsigned int scan_pos;

static unsigned long powers_of_ten[] = {
    1000000000UL, 100000000UL, 10000000UL, 1000000UL, 100000UL,
    10000UL, 1000UL, 100UL, 10UL
};

/* The Code attribute of main */
static unsigned char *code;
static unsigned int max_stack;
static unsigned int max_locals;

/* ---- ROM output ---- */

static void rom_init(void)
{
#asm
    call $0D6B      ; CLS
    ld   a,2
    call $1601      ; CHAN-OPEN, upper screen
#endasm
}

static void rom_putc(unsigned char c) __z88dk_fastcall
{
#asm
    ld   a,l
    rst  16        ; stops at scroll? after each screen of new output, as BASIC does
#endasm
}

/* Stands in for z88dk's console driver, which nothing calls */
void no_console(void)
{
}

/* ---- ROM input, done as the ROM's INPUT LINE does it ---- */

/* Edits a line in the lower screen, leaving it in the workspace, ended by ENTER. */
static void rom_edit_line(void)
{
#asm
    push ix
    ld   hl,23665       ; FLAGX
    set  7,(hl)         ; INPUT LINE: no quotes, no evaluation
    ld   a,1
    call $1601          ; CHAN-OPEN, lower screen
    call $0D6E          ; CLS-LOWER
    ld   (iy+2),1       ; TV-FLAG: the lower screen is in use
    call $16BF          ; SET-WORK: empty the workspace
    ld   hl,23665
    res  6,(hl)         ; a string
    set  5,(hl)         ; input mode
    ld   bc,1
    rst  $30            ; BC-SPACES: room for the final ENTER
    ld   (hl),13
    ld   (23643),hl     ; K-CUR
    call $0F2C          ; EDITOR, returns on ENTER
    ld   (iy+$22),0     ; K-CUR into the ROM, hiding the cursor
    call $21D6          ; IN-CHAN-K
    jr   nz,edited
    call $111D          ; ED-COPY
    ld   bc,(23682)     ; ECHO-E
    call $0DD9          ; CL-SET
.edited
    pop  ix
#endasm
}

/* Clears the lower screen and returns to printing in the upper screen. */
static void rom_end_input(void)
{
#asm
    push ix
    ld   hl,23665       ; FLAGX
    res  5,(hl)
    res  7,(hl)
    ld   bc,(23688)     ; S-POSN, the upper screen print position
    ld   a,(23659)      ; DF-SZ, lines in the lower screen
    cp   b
    jr   c,posn_ok
    ld   c,$21          ; the lower screen grew over it: start of its first line
    ld   b,a
.posn_ok
    ld   (23688),bc
    ld   a,$19
    sub  b
    ld   (23692),a      ; SCR-CT: count scrolls from here, as after INPUT
    res  0,(iy+2)
    call $0DD9          ; CL-SET
    call $0D6E          ; CLS-LOWER
    ld   a,2
    call $1601          ; CHAN-OPEN, upper screen
    pop  ix
#endasm
}

static void print_str(char *s)
{
    while (*s)
        rom_putc(*s++);
}

static void print_newline(void)
{
    rom_putc(13);
}

static void print_int(jint n)
{
    unsigned long v;
    unsigned char i;
    unsigned char digit;
    unsigned char started;

    /*
     * Written this way to avoid two sccz80 miscompilations: 0UL - v computes
     * v - 0, and v % 10UL divides only the low 16 bits of v.
     */
    v = n;
    if (n < 0L) {
        rom_putc('-');
        v = ~v + 1UL;   /* also correct for Integer.MIN_VALUE */
    }
    started = 0;
    for (i = 0; i < 9; i++) {
        digit = 0;
        while (v >= powers_of_ten[i]) {
            v -= powers_of_ten[i];
            digit++;
        }
        if (digit != 0 || started) {
            rom_putc('0' + digit);
            started = 1;
        }
    }
    rom_putc('0' + (unsigned char)v);
}

static void die(char *msg)
{
    print_str("JVM: ");
    print_str(msg);
    print_newline();
    exit(1);
}

/* ---- Class file parsing ---- */

static unsigned int u2(unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | p[1];
}

static jint u4(unsigned char *p)
{
    return ((unsigned long)u2(p) << 16) | (unsigned long)u2(p + 2);
}

/* Sign-extends a bytecode operand; sccz80 loses the sign of a (signed char) cast here. */
static int s1(unsigned char b)
{
    return b < 128 ? (int)b : (int)b - 256;
}

static unsigned char *parse_constant_pool(unsigned char *p)
{
    unsigned int i;

    cp_count = u2(p);
    p += 2;
    if (cp_count > MAX_CP)
        die("constant pool too big");
    for (i = 1; i < cp_count; i++) {
        cp[i] = p;
        switch (*p) {
        case TAG_UTF8:
            p += 3 + u2(p + 1);
            break;
        case 3: case 4: case 9: case 10: case 11: case 12: case 17: case 18:
            p += 5;
            break;
        case 5: case 6:     /* long and double take two pool slots */
            p += 9;
            i++;
            break;
        case 7: case 8: case 16: case 19: case 20:
            p += 3;
            break;
        case 15:
            p += 4;
            break;
        default:
            die("bad constant pool");
        }
    }
    return p;
}

static int utf8_is(unsigned int index, char *s)
{
    unsigned char *e;
    unsigned int len;

    e = cp[index];
    if (*e != TAG_UTF8)
        return 0;
    len = u2(e + 1);
    e += 3;
    while (len != 0 && *s != 0) {
        if (*e++ != (unsigned char)*s++)
            return 0;
        len--;
    }
    return len == 0 && *s == 0;
}

/* Does a Fieldref/Methodref pool entry name this class member? */
static int member_is(unsigned int index, char *cls, char *name, char *desc)
{
    unsigned char *ref;
    unsigned char *nat;

    ref = cp[index];
    nat = cp[u2(ref + 3)];
    return utf8_is(u2(cp[u2(ref + 1)] + 1), cls)
        && utf8_is(u2(nat + 1), name)
        && utf8_is(u2(nat + 3), desc);
}

/*
 * Prints modified UTF-8. Only printable ASCII is shown as itself: the ROM
 * would treat other bytes as control codes, graphics or keywords. Each other
 * character becomes one ?, whatever the length of its encoding.
 */
static void print_chars(unsigned char *s, unsigned int len)
{
    unsigned char c;

    while (len-- != 0) {
        c = *s++;
        if (c == '\n')
            print_newline();
        else if (c >= 32 && c < 127)
            rom_putc(c);
        else if ((c & 0xC0) != 0x80)    /* not a continuation byte */
            rom_putc('?');
    }
}

/* Prints a String or "null". */
static void print_string(jint ref)
{
    unsigned char *s;

    if (ref == 0L) {
        print_str("null");
        return;
    }
    s = DEREF(ref);
    print_chars(s + 3, u2(s + 1));
}

static void print_utf8(unsigned int index)
{
    print_string(REF(cp[index]));
}

/* Stops with the Fieldref/Methodref, written as javap does: class.name:descriptor */
static void die_member(char *what, unsigned int index)
{
    unsigned char *ref;
    unsigned char *nat;

    ref = cp[index];
    nat = cp[u2(ref + 3)];
    print_str("JVM: unsupported ");
    print_str(what);
    rom_putc(' ');
    print_utf8(u2(cp[u2(ref + 1)] + 1));
    rom_putc('.');
    print_utf8(u2(nat + 1));
    rom_putc(':');
    print_utf8(u2(nat + 3));
    print_newline();
    exit(1);
}

/* All sizes fit 16 bits on this machine, so only the low half of a u4 is read. */
static unsigned char *skip_attribute(unsigned char *p)
{
    return p + 6 + u2(p + 4);
}

/* Leaves the code of main in code, max_stack and max_locals; returns the end of the class file. */
static unsigned char *find_main(unsigned char *p)
{
    unsigned int methods;
    unsigned int attrs;
    int is_main;

    p += 6;                     /* access_flags, this_class, super_class */
    p += 2 + 2 * u2(p);         /* interfaces */
    if (u2(p) != 0)
        die("fields not supported");
    p += 2;

    methods = u2(p);
    p += 2;
    while (methods-- != 0) {
        is_main = (u2(p) & ACC_STATIC) != 0
            && utf8_is(u2(p + 2), "main")
            && utf8_is(u2(p + 4), "([Ljava/lang/String;)V");
        attrs = u2(p + 6);
        p += 8;
        while (attrs-- != 0) {
            if (is_main && utf8_is(u2(p), "Code")) {
                max_stack = u2(p + 6);
                max_locals = u2(p + 8);
                code = p + 14;
            }
            p = skip_attribute(p);
        }
    }

    attrs = u2(p);              /* the class's own attributes */
    p += 2;
    while (attrs-- != 0)
        p = skip_attribute(p);

    if (code == 0)
        die("no main method");
    return p;
}

/* ---- Heap and strings ---- */

static unsigned char *alloc(unsigned int size)
{
    unsigned char *p;

    if (size > (unsigned int)(HEAP_END - heap_top))
        die("out of memory");
    p = heap_top;
    heap_top += size;
    return p;
}

/* Allocates a String of len bytes, to be filled in after the 3-byte header. */
static unsigned char *new_string(unsigned int len)
{
    unsigned char *s;

    s = alloc(3 + len);
    s[0] = TAG_UTF8;
    s[1] = len >> 8;
    s[2] = len;
    return s;
}

/* The text of a keyword, ended by its last letter with bit 7 set. */
static unsigned char *keyword(unsigned char token)
{
    unsigned char *p;
    unsigned char n;

    p = TKN_TABLE;
    for (n = token - 0xA4; n != 0; n--) {   /* entry 0 is "?", then RND = 0xA5 */
        while ((*p & 0x80) == 0)
            p++;
        p++;
    }
    return p;
}

/*
 * Copies the edited line to a new String. Keywords become their text, colour
 * controls are dropped and graphics become ?.
 */
static unsigned char *line_to_string(unsigned char *line, unsigned int len)
{
    unsigned char *s;
    unsigned char *k;
    unsigned char *out;
    unsigned int size;
    unsigned int i;
    unsigned char c;
    unsigned char pass;

    for (pass = 0; pass < 2; pass++) {      /* measure, then copy */
        size = 0;
        out = s + 3;
        for (i = 0; i < len; i++) {
            c = line[i];
            if (c >= 0xA5) {
                k = keyword(c);
                do {
                    if (pass)
                        *out++ = *k & 0x7F;
                    size++;
                } while ((*k++ & 0x80) == 0);
            } else if (c >= 0x10 && c <= 0x15) {
                i++;                        /* INK to OVER, and their parameter */
            } else if (c >= 32) {
                if (pass)
                    *out++ = c < 128 ? c : '?';
                size++;
            }
        }
        if (pass == 0)
            s = new_string(size);
    }
    return s;
}

/* Reads a line from the input line, and echoes it in the upper screen. */
static unsigned char *read_line(void)
{
    unsigned char *s;

    rom_edit_line();
    s = line_to_string(DEREF(*WORKSP), *STKBOT - *WORKSP - 1);
    rom_end_input();
    print_string(REF(s));
    print_newline();
    return s;
}

/* ---- Runtime methods ---- */

struct native_class {
    char *name;
    unsigned char kind;     /* OBJ_* */
};

static struct native_class classes[] = {
    { "java/util/Scanner", OBJ_SCANNER }
};

#define CLASS_COUNT (sizeof(classes) / sizeof(classes[0]))

struct native_field {
    char *cls;
    char *name;
    char *desc;
    unsigned char *object;
};

static struct native_field fields[] = {
    { "java/lang/System", "out", "Ljava/io/PrintStream;", system_out },
    { "java/lang/System", "in",  "Ljava/io/InputStream;", system_in }
};

#define FIELD_COUNT (sizeof(fields) / sizeof(fields[0]))

/* args points to the receiver, followed by the arguments; a static method has no receiver */
typedef jint (*native_fn)(jint *args);

struct native {
    char *cls;
    char *name;
    char *desc;
    unsigned char slots;    /* operand stack slots taken, including any receiver */
    unsigned char returns;  /* slots of the result: 1 is the returned value, 2 a double left at args */
    native_fn fn;
};

static jint native_print_int(jint *args)
{
    print_int(args[1]);
    return 0L;
}

static jint native_println_int(jint *args)
{
    print_int(args[1]);
    print_newline();
    return 0L;
}

static char double_text[24];    /* the longest is -1.23456789012345E-308 */
static char *text_end;
static char double_digits[15];

static void put(char c) __z88dk_fastcall
{
    *text_end++ = c;
}

static void put_str(char *s)
{
    while (*s)
        put(*s++);
}

/*
 * Writes a double to double_text as Double.toString does, except with at most
 * 15 significant digits: Java prints as many as it takes to tell the double
 * from its neighbours, up to 17. Returns the length.
 */
static unsigned char format_double(jint *v)
{
    unsigned long high;
    int k;          /* the power of ten of the first digit */
    int low;        /* and of the last one written */
    int exp;
    int p;

    text_end = double_text;
    high = (unsigned long)v[0] & 0x7FFFFFFFUL;
    if (high > 0x7FF00000UL || (high == 0x7FF00000UL && v[1] != 0L))
        put_str("NaN");
    else {
        if (v[0] < 0L)
            put('-');
        if (high == 0x7FF00000UL)
            put_str("Infinity");
        else if (high == 0UL && v[1] == 0L)
            put_str("0.0");
        else {
            k = d_to_decimal(D(v), double_digits);
            low = k - 14;
            while (double_digits[k - low] == '0')
                low++;
            exp = 0;
            if (k < -3 || k >= 7) {     /* not 0.001 to 9999999.99...: as 1.25E-5 */
                exp = k;
                low -= k;
                k = 0;
            }
            if (low > -1)
                low = -1;               /* at least one digit after the point */
            for (p = k > 0 ? k : 0; p >= low; p--) {
                put(p > k ? '0' : double_digits[k - p]);
                if (p == 0)
                    put('.');
            }
            if (exp != 0) {
                put('E');
                if (exp < 0) {
                    put('-');
                    exp = -exp;
                }
                if (exp >= 100)
                    put('0' + exp / 100);
                if (exp >= 10)
                    put('0' + exp / 10 % 10);
                put('0' + exp % 10);
            }
        }
    }
    *text_end = 0;
    return (unsigned char)(text_end - double_text);
}

static jint native_print_double(jint *args)
{
    format_double(args + 1);
    print_str(double_text);
    return 0L;
}

static jint native_println_double(jint *args)
{
    format_double(args + 1);
    print_str(double_text);
    print_newline();
    return 0L;
}

/* Double.toString and String.valueOf */
static jint native_double_to_string(jint *args)
{
    unsigned char *s;
    unsigned char len;

    len = format_double(args);
    s = new_string(len);
    memcpy(s + 3, double_text, len);
    return REF(s);
}

static jint native_print_string(jint *args)
{
    print_string(args[1]);
    return 0L;
}

static jint native_println_string(jint *args)
{
    print_string(args[1]);
    print_newline();
    return 0L;
}

static jint native_println(jint *args)
{
    print_newline();
    return 0L;
}

static jint native_scanner_init(jint *args)
{
    if (args[1] != REF(system_in))
        die("Scanner: only System.in supported");
    return 0L;
}

/* Stops with a message and the text it is about, where Java would throw an exception. */
static void die_text(char *msg, unsigned char *s, unsigned int len)
{
    print_str("JVM: ");
    print_str(msg);
    print_chars(s, len);
    print_newline();
    exit(1);
}

/* Stops as Scanner would throw InputMismatchException. */
static void die_not_int(unsigned char *s, unsigned int len)
{
    die_text("not an int: ", s, len);
}

/* Parses a decimal int with an optional sign, as Scanner.nextInt does. */
static jint parse_int(unsigned char *s, unsigned int len)
{
    unsigned long v;
    unsigned char neg;
    unsigned char last;     /* the largest last digit when v is 214748364 */
    unsigned char d;
    unsigned int i;

    neg = 0;
    i = 0;
    if (len > 1 && (s[0] == '-' || s[0] == '+')) {
        neg = s[0] == '-';
        i = 1;
    }
    last = neg ? 8 : 7;
    v = 0;
    for (; i < len; i++) {
        d = s[i] - '0';
        /* No division or multiplication by 10, to stay clear of sccz80 bugs */
        if (d > 9 || v > 214748364UL || (v == 214748364UL && d > last))
            die_not_int(s, len);
        v = (v << 3) + (v << 1) + d;
    }
    return neg ? (jint)(~v + 1UL) : (jint)v;
}

static jint native_next_line(jint *args)
{
    unsigned char *line;
    unsigned char *s;
    unsigned char *out;
    unsigned int len;

    if (scan_line == 0) {
        scan_line = read_line();
        scan_pos = 0;
    }
    line = scan_line;
    scan_line = 0;
    if (scan_pos == 0)
        return REF(line);
    len = u2(line + 1) - scan_pos;          /* what nextInt left of the line */
    s = new_string(len);
    out = s + 3;
    for (line += 3 + scan_pos; len != 0; len--)
        *out++ = *line++;
    return REF(s);
}

static jint native_next_int(jint *args)
{
    unsigned char *s;
    unsigned int len;
    unsigned int start;

    for (;;) {
        if (scan_line == 0) {
            scan_line = read_line();
            scan_pos = 0;
        }
        s = scan_line + 3;
        len = u2(scan_line + 1);
        while (scan_pos < len && s[scan_pos] == ' ')
            scan_pos++;
        if (scan_pos < len)
            break;
        scan_line = 0;      /* only spaces left, and the end of a line is one too */
    }
    start = scan_pos;
    while (scan_pos < len && s[scan_pos] != ' ')
        scan_pos++;
    return parse_int(s + start, scan_pos - start);
}

static jint native_close(jint *args)
{
    return 0L;
}

static jint native_parse_int(jint *args)
{
    unsigned char *s;

    if (args[0] == 0L)
        die("not an int: null");
    s = DEREF(args[0]);
    if (u2(s + 1) == 0)
        die_not_int(s + 3, 0);
    return parse_int(s + 3, u2(s + 1));
}

/* Does s[i..len) start with the ASCII text t? */
static int starts_with(unsigned char *s, unsigned int i, unsigned int len, char *t)
{
    while (*t != 0) {
        if (i >= len || s[i++] != (unsigned char)*t++)
            return 0;
    }
    return i == len;
}

/*
 * Parses a double as Double.parseDouble does, rounded correctly. Rounding
 * needs only one multiplication or division when the number has at most 15
 * significant digits and a power of ten of at most 22, as both are then
 * exact doubles; other numbers stop the program. Hexadecimal is unsupported.
 */
static void parse_double(jint *r, unsigned char *s, unsigned int len)
{
    jint ten[2];
    jint digit[2];
    unsigned int i;
    unsigned char neg;
    unsigned char dot;
    unsigned char seen;         /* a digit */
    unsigned char digits;       /* significant digits in r */
    unsigned char exp_neg;
    unsigned char d;
    int exp10;
    int e;

    while (len != 0 && s[len - 1] <= ' ')  /* as String.trim() */
        len--;
    i = 0;
    while (i < len && s[i] <= ' ')
        i++;
    neg = 0;
    if (i < len && (s[i] == '+' || s[i] == '-')) {
        neg = s[i] == '-';
        i++;
    }
    if (starts_with(s, i, len, "NaN")) {
        r[0] = 0x7FF80000L;
        r[1] = 0L;
        return;
    }
    if (starts_with(s, i, len, "Infinity")) {
        r[0] = neg ? (jint)0xFFF00000UL : 0x7FF00000L;
        r[1] = 0L;
        return;
    }

    d_from_int(D(r), 0L);
    d_from_int(D(ten), 10L);
    dot = 0;
    seen = 0;
    digits = 0;
    exp10 = 0;
    for (; i < len; i++) {
        if (s[i] == '.' && !dot) {
            dot = 1;
            continue;
        }
        d = s[i] - '0';
        if (d > 9)
            break;
        seen = 1;
        if (digits == 0 && d == 0) {        /* a leading zero */
            if (dot)
                exp10--;
        } else if (digits < 15) {
            d_mul(D(r), D(r), D(ten));      /* exact: r stays below 10^15 */
            d_from_int(D(digit), (jint)d);
            d_add(D(r), D(r), D(digit));
            digits++;
            if (dot)
                exp10--;
        } else if (d != 0) {
            die_text("unsupported double: ", s, len);
        } else if (!dot) {
            exp10++;                        /* a trailing zero */
        }
    }
    if (!seen)
        die_text("not a double: ", s, len);
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        exp_neg = 0;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            exp_neg = s[i] == '-';
            i++;
        }
        if (i == len)
            die_text("not a double: ", s, len);
        e = 0;
        for (; i < len && (d = s[i] - '0') <= 9; i++) {
            if (e < 1000)
                e = (e << 3) + (e << 1) + d;
        }
        exp10 += exp_neg ? -e : e;
    }
    if (i < len && (s[i] == 'd' || s[i] == 'D' || s[i] == 'f' || s[i] == 'F'))
        i++;
    if (i != len)
        die_text("not a double: ", s, len);

    if (digits != 0) {
        if (exp10 > 22 || exp10 < -22)
            die_text("unsupported double: ", s, len);
        d_from_int(D(digit), 1L);           /* 10^|exp10|, exact */
        for (e = exp10 < 0 ? -exp10 : exp10; e != 0; e--)
            d_mul(D(digit), D(digit), D(ten));
        if (exp10 < 0)
            d_div(D(r), D(r), D(digit));
        else
            d_mul(D(r), D(r), D(digit));
    }
    if (neg)
        d_neg(D(r), D(r));
}

/* Returns its double in args[0] and args[1], as natives[] says it takes two slots */
static jint native_parse_double(jint *args)
{
    unsigned char *s;

    if (args[0] == 0L)
        die("not a double: null");
    s = DEREF(args[0]);
    parse_double(args, s + 3, u2(s + 1));
    return 0L;
}

static jint native_abs(jint *args)
{
    return args[0] < 0L ? ~args[0] + 1L : args[0];  /* Integer.MIN_VALUE stays negative */
}

static jint native_max(jint *args)
{
    return args[0] > args[1] ? args[0] : args[1];
}

static jint native_min(jint *args)
{
    return args[0] < args[1] ? args[0] : args[1];
}

static struct native natives[] = {
    { "java/io/PrintStream", "print",    "(I)V",                     2, 0, native_print_int },
    { "java/io/PrintStream", "println",  "(I)V",                     2, 0, native_println_int },
    { "java/io/PrintStream", "print",    "(Ljava/lang/String;)V",    2, 0, native_print_string },
    { "java/io/PrintStream", "println",  "(Ljava/lang/String;)V",    2, 0, native_println_string },
    { "java/io/PrintStream", "print",    "(D)V",                     3, 0, native_print_double },
    { "java/io/PrintStream", "println",  "(D)V",                     3, 0, native_println_double },
    { "java/io/PrintStream", "println",  "()V",                      1, 0, native_println },
    { "java/util/Scanner",   "<init>",   "(Ljava/io/InputStream;)V", 2, 0, native_scanner_init },
    { "java/util/Scanner",   "nextLine", "()Ljava/lang/String;",     1, 1, native_next_line },
    { "java/util/Scanner",   "nextInt",  "()I",                      1, 1, native_next_int },
    { "java/util/Scanner",   "close",    "()V",                      1, 0, native_close },
    { "java/lang/Integer",   "parseInt", "(Ljava/lang/String;)I",    1, 1, native_parse_int },
    { "java/lang/Double",    "parseDouble", "(Ljava/lang/String;)D", 1, 2, native_parse_double },
    { "java/lang/Double",    "toString", "(D)Ljava/lang/String;",    2, 1, native_double_to_string },
    { "java/lang/String",    "valueOf",  "(D)Ljava/lang/String;",    2, 1, native_double_to_string },
    { "java/lang/Math",      "abs",      "(I)I",                     1, 1, native_abs },
    { "java/lang/Math",      "max",      "(II)I",                    2, 1, native_max },
    { "java/lang/Math",      "min",      "(II)I",                    2, 1, native_min }
};

#define NATIVE_COUNT (sizeof(natives) / sizeof(natives[0]))

/* Allocates an object of a class in classes[], given its Class pool entry. */
static unsigned char *new_object(unsigned int index)
{
    unsigned int name;
    unsigned char i;
    unsigned char *obj;

    name = u2(cp[index] + 1);
    for (i = 0; i < CLASS_COUNT; i++) {
        if (utf8_is(name, classes[i].name)) {
            obj = alloc(1);
            *obj = classes[i].kind;
            return obj;
        }
    }
    print_str("JVM: unsupported class ");
    print_utf8(name);
    print_newline();
    exit(1);
    return 0;
}

static unsigned char *get_static(unsigned int index)
{
    unsigned char i;
    struct native_field *f;

    for (i = 0; i < FIELD_COUNT; i++) {
        f = &fields[i];
        if (member_is(index, f->cls, f->name, f->desc))
            return f->object;
    }
    die_member("field", index);
    return 0;
}

/* Looks a Methodref up in natives[] once, then remembers the result. */
static struct native *resolve_method(unsigned int index)
{
    unsigned char i;
    struct native *m;

    if (resolved[index] == 0) {
        for (i = 0; i < NATIVE_COUNT; i++) {
            m = &natives[i];
            if (member_is(index, m->cls, m->name, m->desc)) {
                resolved[index] = i + 1;
                return m;
            }
        }
        die_member("method", index);
    }
    return &natives[resolved[index] - 1];
}

/* ---- Interpreter ---- */

/*
 * Divides as Java does, truncating towards zero, with the remainder taking
 * the sign of a. Works on magnitudes, because C leaves Integer.MIN_VALUE / -1
 * undefined; in Java it wraps around to Integer.MIN_VALUE, with remainder 0.
 */
static jint divide(jint a, jint b, unsigned char remainder)
{
    unsigned long ua;
    unsigned long ub;
    unsigned long r;

    if (b == 0L)
        die("/ by zero");
    ua = a < 0L ? ~(unsigned long)a + 1UL : (unsigned long)a;
    ub = b < 0L ? ~(unsigned long)b + 1UL : (unsigned long)b;
    if (remainder) {
        r = ua % ub;
        return a < 0L ? (jint)(~r + 1UL) : (jint)r;
    }
    r = ua / ub;
    return (a < 0L) != (b < 0L) ? (jint)(~r + 1UL) : (jint)r;
}

static int branch_taken(unsigned char op, jint a, jint b)
{
    switch (op) {
    case OP_IFEQ: case OP_IF_ICMPEQ: case OP_IF_ACMPEQ: case OP_IFNULL:
        return a == b;
    case OP_IFNE: case OP_IF_ICMPNE: case OP_IF_ACMPNE: case OP_IFNONNULL:
        return a != b;
    case OP_IFLT: case OP_IF_ICMPLT: return a < b;
    case OP_IFGE: case OP_IF_ICMPGE: return a >= b;
    case OP_IFGT: case OP_IF_ICMPGT: return a > b;
    }
    return a <= b;      /* OP_IFLE, OP_IF_ICMPLE */
}

static void run(void)
{
    unsigned char *pc;
    unsigned char *e;
    jint *sp;
    jint a;
    jint b;
    unsigned char op;
    unsigned char n;
    struct native *m;

    if (max_stack + max_locals > MAX_FRAME)
        die("frame too big");

    frame[0] = 0L;      /* args: arrays do not exist here, so main gets null */
    sp = frame + max_locals;
    pc = code;

    for (;;) {
        op = *pc;
        switch (op) {
        case OP_ACONST_NULL:
            *sp++ = 0L;
            pc++;
            break;
        case OP_ICONST_M1: case OP_ICONST_0: case OP_ICONST_1: case OP_ICONST_2:
        case OP_ICONST_3: case OP_ICONST_4: case OP_ICONST_5:
            *sp++ = (jint)((int)op - OP_ICONST_0);
            pc++;
            break;
        case OP_BIPUSH:
            *sp++ = (jint)s1(pc[1]);
            pc += 2;
            break;
        case OP_SIPUSH:
            *sp++ = (jint)(int)u2(pc + 1);
            pc += 3;
            break;
        case OP_DCONST_0: case OP_DCONST_1:
            d_from_int(D(sp), (jint)(op - OP_DCONST_0));
            sp += 2;
            pc++;
            break;
        case OP_LDC2_W:
            e = cp[u2(pc + 1)];
            if (*e != TAG_DOUBLE)
                die("ldc2_w: only double supported");
            sp[0] = u4(e + 1);
            sp[1] = u4(e + 5);
            sp += 2;
            pc += 3;
            break;
        case OP_LDC:
            e = cp[pc[1]];
            if (*e == TAG_INTEGER)
                *sp++ = u4(e + 1);
            else if (*e == TAG_STRING)
                *sp++ = REF(cp[u2(e + 1)]);
            else
                die("ldc: only int and String supported");
            pc += 2;
            break;
        case OP_ILOAD: case OP_ALOAD:   /* references and ints share the 32-bit slots */
            *sp++ = frame[pc[1]];
            pc += 2;
            break;
        case OP_ILOAD_0: case OP_ILOAD_1: case OP_ILOAD_2: case OP_ILOAD_3:
            *sp++ = frame[op - OP_ILOAD_0];
            pc++;
            break;
        case OP_ALOAD_0: case OP_ALOAD_1: case OP_ALOAD_2: case OP_ALOAD_3:
            *sp++ = frame[op - OP_ALOAD_0];
            pc++;
            break;
        case OP_ISTORE: case OP_ASTORE:
            frame[pc[1]] = *--sp;
            pc += 2;
            break;
        case OP_ISTORE_0: case OP_ISTORE_1: case OP_ISTORE_2: case OP_ISTORE_3:
            frame[op - OP_ISTORE_0] = *--sp;
            pc++;
            break;
        case OP_ASTORE_0: case OP_ASTORE_1: case OP_ASTORE_2: case OP_ASTORE_3:
            frame[op - OP_ASTORE_0] = *--sp;
            pc++;
            break;
        case OP_DLOAD: case OP_DLOAD_0: case OP_DLOAD_1: case OP_DLOAD_2: case OP_DLOAD_3:
            if (op == OP_DLOAD) {
                n = pc[1];
                pc += 2;
            } else {
                n = op - OP_DLOAD_0;
                pc++;
            }
            sp[0] = frame[n];
            sp[1] = frame[n + 1];
            sp += 2;
            break;
        case OP_DSTORE: case OP_DSTORE_0: case OP_DSTORE_1: case OP_DSTORE_2: case OP_DSTORE_3:
            if (op == OP_DSTORE) {
                n = pc[1];
                pc += 2;
            } else {
                n = op - OP_DSTORE_0;
                pc++;
            }
            sp -= 2;
            frame[n] = sp[0];
            frame[n + 1] = sp[1];
            break;
        case OP_IADD:
            b = *--sp;
            sp[-1] = sp[-1] + b;
            pc++;
            break;
        case OP_ISUB:
            b = *--sp;
            sp[-1] = sp[-1] - b;
            pc++;
            break;
        case OP_IMUL:
            b = *--sp;
            sp[-1] = sp[-1] * b;    /* the low 32 bits, so signedness does not matter */
            pc++;
            break;
        case OP_IDIV: case OP_IREM:
            b = *--sp;
            sp[-1] = divide(sp[-1], b, op == OP_IREM);
            pc++;
            break;
        case OP_INEG:
            sp[-1] = ~sp[-1] + 1L;
            pc++;
            break;
        case OP_DADD: case OP_DSUB: case OP_DMUL: case OP_DDIV:
            sp -= 2;
            if (op == OP_DADD)
                d_add(D(sp - 2), D(sp - 2), D(sp));
            else if (op == OP_DSUB)
                d_sub(D(sp - 2), D(sp - 2), D(sp));
            else if (op == OP_DMUL)
                d_mul(D(sp - 2), D(sp - 2), D(sp));
            else
                d_div(D(sp - 2), D(sp - 2), D(sp));
            pc++;
            break;
        case OP_DNEG:
            d_neg(D(sp - 2), D(sp - 2));
            pc++;
            break;
        case OP_I2D:
            d_from_int(D(sp - 1), sp[-1]);
            sp++;
            pc++;
            break;
        case OP_D2I:
            sp[-2] = d_to_int(D(sp - 2));
            sp--;
            pc++;
            break;
        case OP_DCMPL: case OP_DCMPG:  /* they differ only for NaN */
            sp -= 3;
            sp[-1] = (jint)d_cmp(D(sp - 1), D(sp + 1), op == OP_DCMPG ? 1 : -1);
            pc++;
            break;
        case OP_IINC:
            frame[pc[1]] = frame[pc[1]] + (jint)s1(pc[2]);
            pc += 3;
            break;
        case OP_IFEQ: case OP_IFNE: case OP_IFLT:
        case OP_IFGE: case OP_IFGT: case OP_IFLE:
        case OP_IFNULL: case OP_IFNONNULL:  /* null is 0 */
            a = *--sp;
            if (branch_taken(op, a, 0L))
                pc += (int)u2(pc + 1);
            else
                pc += 3;
            break;
        case OP_IF_ICMPEQ: case OP_IF_ICMPNE: case OP_IF_ICMPLT:
        case OP_IF_ICMPGE: case OP_IF_ICMPGT: case OP_IF_ICMPLE:
        case OP_IF_ACMPEQ: case OP_IF_ACMPNE:
            b = *--sp;
            a = *--sp;
            if (branch_taken(op, a, b))
                pc += (int)u2(pc + 1);
            else
                pc += 3;
            break;
        case OP_GOTO:
            pc += (int)u2(pc + 1);
            break;
        case OP_RETURN:
            return;
        case OP_GETSTATIC:
            *sp++ = REF(get_static(u2(pc + 1)));
            pc += 3;
            break;
        case OP_INVOKEVIRTUAL: case OP_INVOKESPECIAL:
            m = resolve_method(u2(pc + 1));
            sp = &sp[-(int)m->slots];
            if (*sp == 0L)
                die("null pointer");
            a = m->fn(sp);
            if (m->returns == 1)
                *sp = a;
            sp += m->returns;
            pc += 3;
            break;
        case OP_INVOKESTATIC:
            m = resolve_method(u2(pc + 1));
            sp = &sp[-(int)m->slots];
            a = m->fn(sp);
            if (m->returns == 1)
                *sp = a;
            sp += m->returns;
            pc += 3;
            break;
        case OP_NEW:
            *sp++ = REF(new_object(u2(pc + 1)));
            pc += 3;
            break;
        case OP_DUP:
            *sp = sp[-1];
            sp++;
            pc++;
            break;
        case OP_DUP2:
            sp[0] = sp[-2];
            sp[1] = sp[-1];
            sp += 2;
            pc++;
            break;
        case OP_POP:
            --sp;
            pc++;
            break;
        case OP_POP2:
            sp -= 2;
            pc++;
            break;
        default:
            print_str("JVM: unsupported opcode ");
            print_int((jint)op);
            print_str(" at pc ");
            print_int((jint)(int)(pc - code));
            print_newline();
            return;
        }
    }
}

int main(void)
{
    rom_init();
    if (u2(CLASS_ADDR) != 0xCAFE || u2(CLASS_ADDR + 2) != 0xBABE)
        die("no class file at 49152");
    heap_top = find_main(parse_constant_pool(CLASS_ADDR + 8));
    run();
    return 0;
}
