/*
 * zxjvm - a very small JVM for the ZX Spectrum.
 *
 * Runs public static void main(String[]) of a single .class file that has
 * already been loaded into memory at CLASS_ADDR. Bytecode is interpreted
 * directly from the class file image; there is no verifier, no heap and no
 * class loading. Runtime methods are implemented in C, listed in natives[].
 *
 * A reference is the 16-bit address of an object, held in a 32-bit slot, and
 * null is 0. A String is its CONSTANT_Utf8 entry in the class file, so the
 * first byte of an object is TAG_UTF8 for a String or an OBJ_* kind otherwise.
 *
 * Memory map: 32768 this program, 49152 the class file.
 */

#pragma output CRT_ORG_CODE = 32768
#pragma output CRT_ENABLE_STDIO = 0
#pragma output CLIB_EXIT_STACK_SIZE = 0
#pragma output CLIB_MALLOC_HEAP_SIZE = 0
#pragma output CLIB_STDIO_HEAP_SIZE = 0

#include <stdlib.h>

#define CLASS_ADDR ((unsigned char *)49152)

#define MAX_CP     256  /* constant pool entries */
#define MAX_FRAME  64   /* locals + operand stack, in 32-bit slots */

#define ACC_STATIC 0x0008

#define TAG_UTF8    1
#define TAG_INTEGER 3
#define TAG_STRING  8

#define OBJ_PRINTSTREAM 0x80

#define REF(p)   ((jint)(unsigned int)(p))
#define DEREF(r) ((unsigned char *)(unsigned int)(r))

#define OP_ACONST_NULL   0x01
#define OP_ICONST_M1     0x02
#define OP_ICONST_0      0x03
#define OP_ICONST_1      0x04
#define OP_ICONST_2      0x05
#define OP_ICONST_3      0x06
#define OP_ICONST_4      0x07
#define OP_ICONST_5      0x08
#define OP_BIPUSH        0x10
#define OP_SIPUSH        0x11
#define OP_LDC           0x12
#define OP_ILOAD         0x15
#define OP_ALOAD         0x19
#define OP_ILOAD_0       0x1A
#define OP_ILOAD_1       0x1B
#define OP_ILOAD_2       0x1C
#define OP_ILOAD_3       0x1D
#define OP_ALOAD_0       0x2A
#define OP_ALOAD_1       0x2B
#define OP_ALOAD_2       0x2C
#define OP_ALOAD_3       0x2D
#define OP_ISTORE        0x36
#define OP_ASTORE        0x3A
#define OP_ISTORE_0      0x3B
#define OP_ISTORE_1      0x3C
#define OP_ISTORE_2      0x3D
#define OP_ISTORE_3      0x3E
#define OP_ASTORE_0      0x4B
#define OP_ASTORE_1      0x4C
#define OP_ASTORE_2      0x4D
#define OP_ASTORE_3      0x4E
#define OP_IADD          0x60
#define OP_ISUB          0x64
#define OP_IINC          0x84
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
#define OP_IFNULL        0xC6
#define OP_IFNONNULL     0xC7

typedef long jint;      /* sccz80: int is 16 bits, long is 32 bits */

static unsigned char *cp[MAX_CP];
static unsigned int cp_count;
static unsigned char resolved[MAX_CP];  /* Methodref -> natives[] index + 1 */
static jint frame[MAX_FRAME];

static unsigned char system_out[] = { OBJ_PRINTSTREAM };

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
 * Prints a String or "null". Only printable ASCII is shown as itself: the ROM
 * would treat other bytes as control codes, graphics or keywords. Each other
 * character becomes one ?, whatever the length of its modified UTF-8 encoding.
 */
static void print_string(jint ref)
{
    unsigned char *s;
    unsigned int len;
    unsigned char c;

    if (ref == 0L) {
        print_str("null");
        return;
    }
    s = DEREF(ref);
    len = u2(s + 1);
    s += 3;
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

/* Leaves the code of main in code, max_stack and max_locals. */
static void find_main(unsigned char *p)
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
                return;
            }
            p = skip_attribute(p);
        }
    }
    die("no main method");
}

/* ---- Runtime methods ---- */

/* args points to the receiver, followed by the arguments */
typedef void (*native_fn)(jint *args);

struct native {
    char *cls;
    char *name;
    char *desc;
    unsigned char slots;    /* operand stack slots taken, including the receiver */
    native_fn fn;
};

static void native_print_int(jint *args)
{
    print_int(args[1]);
}

static void native_println_int(jint *args)
{
    print_int(args[1]);
    print_newline();
}

static void native_print_string(jint *args)
{
    print_string(args[1]);
}

static void native_println_string(jint *args)
{
    print_string(args[1]);
    print_newline();
}

static void native_println(jint *args)
{
    print_newline();
}

static struct native natives[] = {
    { "java/io/PrintStream", "print",   "(I)V",                  2, native_print_int },
    { "java/io/PrintStream", "println", "(I)V",                  2, native_println_int },
    { "java/io/PrintStream", "print",   "(Ljava/lang/String;)V", 2, native_print_string },
    { "java/io/PrintStream", "println", "(Ljava/lang/String;)V", 2, native_println_string },
    { "java/io/PrintStream", "println", "()V",                   1, native_println }
};

#define NATIVE_COUNT (sizeof(natives) / sizeof(natives[0]))

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
        case OP_LDC:
            e = cp[pc[1]];
            if (*e == TAG_INTEGER)
                *sp++ = ((unsigned long)u2(e + 1) << 16) | (unsigned long)u2(e + 3);
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
            if (!member_is(u2(pc + 1), "java/lang/System", "out", "Ljava/io/PrintStream;"))
                die_member("field", u2(pc + 1));
            *sp++ = REF(system_out);
            pc += 3;
            break;
        case OP_INVOKEVIRTUAL:
            m = resolve_method(u2(pc + 1));
            sp = &sp[-(int)m->slots];
            if (*sp == 0L)
                die("null pointer");
            m->fn(sp);
            pc += 3;
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
    find_main(parse_constant_pool(CLASS_ADDR + 8));
    run();
    return 0;
}
