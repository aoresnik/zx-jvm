# zx-jvm

A very small JVM for the ZX Spectrum. It reads a real `.class` file produced by `javac`, loaded from tape as a CODE block, and interprets its bytecode on the Z80.

It is basic because of the constraints of the target machine: currently enough to run programs like [`examples/Fibonacci.java`](examples/Fibonacci.java), which prints the first 15 Fibonacci numbers, to print strings, to read lines and numbers with `Scanner` on the input line at the bottom of the screen, and to calculate with `double`, as [`examples/Loan.java`](examples/Loan.java) does for a loan's monthly payments. It can also draw: a program that paints a `java.awt.Canvas` in a `JFrame`, such as [`examples/Shapes.java`](examples/Shapes.java), runs unchanged on a desktop and on the Spectrum.

Of course it was vibecoded.

[`HelloWorld.java`](examples/HelloWorld.java):

![Hello world from Java on a Spectrum screen](docs/HelloWorld.png)

[`Fibonacci.java`](examples/Fibonacci.java):

![The first 15 Fibonacci numbers on a Spectrum screen](docs/Fibonacci.png)

[`Loan.java`](examples/Loan.java), for a 15-year loan:

![The monthly payment and yearly balances of a 15-year loan on a Spectrum screen](docs/Loan.png)

[`Shapes.java`](examples/Shapes.java), on the Spectrum and, painted by Java2D, on a desktop:

![Lines, rectangles, circles and text drawn on a Spectrum screen](docs/Shapes.png)

![The same drawing painted by Java on a desktop](docs/Shapes-desktop.png)

## Requirements

- [z88dk](https://z88dk.org/), with `zcc` and `appmake` on your `PATH`
- A JDK, version 11 or newer
- [Fuse](https://fuse-emulator.sourceforge.net/) to run the tapes on an emulated Spectrum
- GNU Make
- Python 3 and a C compiler for the host, such as GCC, for the [tests](#tests)

Tested with z88dk 2.0, OpenJDK 17, Fuse 1.6.0, Python 3.11, SkoolKit 10.2 and GCC 12.

## Quick start

```sh
make        # builds build/Fibonacci.tap and build/zx-jvm.tap
make run    # opens build/Fibonacci.tap in Fuse
```

The program prints 0 to 377 and returns to BASIC with `0 OK, 40:1`.

For the classic first program, [`examples/HelloWorld.java`](examples/HelloWorld.java), which prints `Hello world from Java`:

```sh
make run CLASS=HelloWorld
```

[`examples/Adder.java`](examples/Adder.java) reads input. It asks for your name, then adds up numbers until you enter 0 and shows their count, total and largest value:

```sh
make run CLASS=Adder
```

[`examples/Loan.java`](examples/Loan.java) calculates the monthly payment of a fixed-rate loan from its amount, yearly interest rate and length in years, and lists the balance left after each year:

```sh
make run CLASS=Loan
```

[`examples/Shapes.java`](examples/Shapes.java) draws lines, rectangles, circles and text. It runs as it is on a desktop too, with `java examples/Shapes.java`:

```sh
make run CLASS=Shapes
```

## Running your own class

Your class must be in the default package and stay within the [supported features](#what-is-supported).

### Build a complete tape

This is the simplest way, and gives you a tape that loads and runs without any prompt:

```sh
make CLASS=MyProgram JAVA_DIR=path/to/source
```

The result is `build/MyProgram.tap`. To open it in Fuse straight away, use the `run` target with the same arguments.

### Use the JVM-only tape

`build/zx-jvm.tap` contains the JVM without a class. After loading, it shows `Insert class tape, press a key` and then loads the next CODE block as the class.

First, put the class in a tape of its own:

```sh
javac -d build MyProgram.java
appmake +zx --noloader --org 49152 -b build/MyProgram.class --blockname MyProgram -o MyProgram.tap
```

Then do one of the following:

- **Swap tapes:** load `zx-jvm.tap`. At the prompt, open `MyProgram.tap` (F7 in Fuse) and press a key.
- **Join the tapes:** run `cat build/zx-jvm.tap MyProgram.tap > combined.tap` and load the result. You still press a key at the prompt.

The class always loads right after the JVM, whatever address its block was saved with.

## What is supported

The JVM runs `public static void main(String[])` from one class, and the methods of that class it calls.

| Area | Supported |
|---|---|
| Constants | `iconst_m1` to `iconst_5`, `bipush`, `sipush`, `ldc` (int and String), `aconst_null`, `dconst_0`, `dconst_1`, `ldc2_w` (double) |
| Local variables | `iload`, `istore`, `aload`, `astore`, `dload`, `dstore` and their `_0` to `_3` forms, `iinc` |
| Arithmetic | `iadd`, `isub`, `imul`, `idiv`, `irem`, `ineg`, with 32-bit wraparound as in Java; `dadd`, `dsub`, `dmul`, `ddiv`, `dneg` |
| Conversions and comparisons | `i2d`, `d2i`, `dcmpl`, `dcmpg` |
| Branches | `ifeq`, `ifne`, `iflt`, `ifge`, `ifgt`, `ifle`, all `if_icmp` forms, `ifnull`, `ifnonnull`, `if_acmpeq`, `if_acmpne`, `goto` |
| Methods | Static and instance methods of the class, including recursion and constructors, called with `invokestatic`, `invokevirtual` and `invokespecial`; `return`, `ireturn`, `areturn`, `dreturn` |
| Objects and calls | `new`, `dup`, `dup2`, `pop`, `pop2`, `getstatic`, for the class itself and the runtime classes below |
| Graphics | A `java.awt.Canvas` shown in a `javax.swing.JFrame`, and the drawing methods of `java.awt.Graphics` listed in [Graphics](#graphics) |
| Runtime | `System.out.print` and `println` of `int`, `double` and `String`, and `println()`; `new Scanner(System.in)` with `nextLine()`, `nextInt()` and `close()`; `Integer.parseInt(String)`; `Double.parseDouble(String)`; `Double.toString(double)` and `String.valueOf(double)`; `Math.abs`, `Math.max` and `Math.min` of `int` |

Your methods can take and return `int`, `boolean`, `double`, `String` and objects of your class. Objects of your class have no fields, so they are useful mainly to call instance methods on, as when a class extends a runtime class and overrides its methods. A method your class doesn't declare, but inherits from a runtime class, is looked up among the runtime methods by its name and descriptor.

Strings are constants or lines read with `Scanner`. They can be stored in local variables and compared with `==` and `!=`. As in Java, equal string literals in a class are the same object. Constant expressions such as `"a" + "b"` work because `javac` joins them, but `"x = " + x` needs `invokedynamic` and is unsupported; print the parts separately instead.

Output uses the Spectrum character set, which is close to ASCII:

- `\n` starts a new line.
- `^`, `` ` `` and code 127 print as `↑`, `£` and `?`.
- Other control characters and every non-ASCII character print as one `?` each. Characters outside the Basic Multilingual Plane, such as emoji, print as `??`, because Java stores them as two `char`s.
- Long lines wrap at 32 columns.

The Spectrum has no scrollback. Instead, as in BASIC, output stops at `scroll?` after each screen of new text:

- Any key except N, SPACE and BREAK shows the next screen.
- N, SPACE or BREAK stop the program with `D BREAK - CONT repeats`. `CONT` then runs the program again from the start.

### Input

`Scanner` reads from the input line at the bottom of the screen, as BASIC's `INPUT LINE` does:

```java
Scanner in = new Scanner(System.in);
System.out.print("Name: ");
String name = in.nextLine();
```

- **Editing:** you edit the line with the Spectrum's own editor, including DELETE and the cursor keys, and press ENTER to finish.
- **Echo:** the line is then copied to the upper screen after the prompt, as a terminal would show it, so the screen keeps a record of the conversation.
- **STOP:** cursor down (CAPS SHIFT + 6) stops the program with `H STOP in INPUT`, as in BASIC.
- **Keywords:** keyword symbols such as `<=` become their text.
- **Other characters:** graphics and user-defined graphics become `?`, and colour controls are dropped.

`Scanner` behaves as on any JVM:

- `nextInt()` skips spaces and empty lines, reads one number and leaves the rest of the line.
- A following `nextLine()` returns that rest, often an empty string, without waiting for input.
- If the input isn't a number in the range of `int`, the program stops with an error, because exceptions are unsupported. Java would throw `InputMismatchException`.

Division by zero stops the program with `JVM: / by zero`, where Java would throw `ArithmeticException`. `Integer.parseInt` accepts what `nextInt()` accepts, without spaces.

### Doubles

`double` follows IEEE 754 exactly, as Java requires. Every result is rounded the same way as on any other JVM, including infinities, NaN, `-0.0` and the smallest (subnormal) numbers. The arithmetic is done in software, so it is slow: on a real Spectrum, an addition takes about 4 ms, and a multiplication or a division about 14 ms. `Loan` works a year at a time rather than a month at a time to stay quick.

`System.out.println(d)`, `Double.toString(d)` and `String.valueOf(d)` print a `double` as Java does, except for the number of digits. Java prints as many digits as it takes to tell the `double` apart from its neighbours, up to 17. Finding those exactly needs arithmetic on numbers of up to about 1100 bits, which would take too much code. The JVM rounds to at most 15 significant digits instead, then drops trailing zeros. Any decimal of up to 15 digits, such as anything typed in, prints as written:

| Value | Java | zx-jvm |
|---|---|---|
| `0.1`, `825.37`, `1e-5` | `0.1`, `825.37`, `1.0E-5` | The same |
| `1.0 / 3` | `0.3333333333333333` | `0.333333333333333` |
| `0.1 + 0.2` | `0.30000000000000004` | `0.3` |
| `Double.MAX_VALUE` | `1.7976931348623157E308` | `1.79769313486232E308` |
| `Double.MIN_VALUE` | `4.9E-324` | `4.94065645841247E-324` |

So two different `double`s can print the same, as `0.1 + 0.2` and `0.3` do, which matters when you compare doubles: compare them in the program, not by their printed text. The layout follows Java: from 0.001 up to 10,000,000 without an exponent, as `1234.5`, and otherwise as `1.2345E7`, with at least one digit after the point. In rare cases, when a value is within a thousandth of a unit from halfway between two 15-digit decimals, the last digit is rounded the other way. Printing a number takes about 40 to 60 ms, and up to 140 ms for very large or very small exponents.

Java's `println` doesn't format money: `825.5` prints as `825.5`, not `825.50`, and `String.format` is unsupported. To print a fixed number of decimals, convert to whole units, as `Loan` does for cents:

```java
int cents = (int) (amount * 100 + 0.5);
System.out.print(cents / 100);
System.out.print(cents % 100 < 10 ? ".0" : ".");
System.out.println(cents % 100);
```

To read a `double`, use `Double.parseDouble(in.nextLine())`. `Scanner.nextDouble()` is unsupported. On a desktop, it follows the locale, so in many countries it would expect `4,5` instead of `4.5`; `Double.parseDouble` reads `4.5` everywhere.

`Double.parseDouble` accepts what Java accepts, except for hexadecimal, and rounds correctly. To keep the code small, it supports at most 15 significant digits and powers of ten from -22 to 22, which covers ordinary input. Other numbers stop the program with `JVM: unsupported double`, and text that isn't a number stops it with `JVM: not a double`, where Java would throw `NumberFormatException`.

### Graphics

A program draws as it would in a window on a desktop: its class extends `Canvas` and overrides `paint`, and `main` shows it in a `JFrame`. The canvas is the whole screen, 256 by 192 pixels, with (0, 0) at the top left as in Java:

```java
public class Shapes extends Canvas {
    public void paint(Graphics g) {
        g.setColor(Color.RED);
        g.drawOval(96, 24, 72, 72);
        g.drawString("Shapes in Java", 72, 14);
    }

    public static void main(String[] args) {
        JFrame frame = new JFrame("Shapes");
        Shapes shapes = new Shapes();
        shapes.setBackground(Color.WHITE);
        shapes.setPreferredSize(new Dimension(256, 192));
        frame.add(shapes);
        frame.pack();
        frame.setResizable(false);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setVisible(true);
    }
}
```

`setVisible(true)` clears the screen to the background colour and calls `paint` once. When `main` returns, the picture stays on the screen until you press a key, as the window would stay open on a desktop, and then the program returns to BASIC. The window is a `JFrame` rather than AWT's `Frame` because a `Frame` only closes with a listener, which would be a second class.

| Class | Supported |
|---|---|
| `JFrame` | Constructors with and without a title; `add`, `pack`, `setResizable`, `setDefaultCloseOperation`, `setVisible`. Only `setVisible` does anything on the Spectrum |
| `Canvas` | Its constructor, `setBackground`, and `setPreferredSize` with a `new Dimension(w, h)`, which makes no difference here |
| `Graphics` | `setColor`, `drawLine`, `drawRect`, `fillRect`, `clearRect`, `drawOval`, `fillOval`, `drawString` |
| `Color` | `BLACK`, `BLUE`, `RED`, `MAGENTA`, `GREEN`, `CYAN`, `YELLOW` and `WHITE`: the Spectrum's colours, in bright, as Java's are at full strength |

There is no `drawPoint` in Java: `g.drawLine(x, y, x, y)` draws one pixel. Circles are ovals with equal width and height.

The pixels are those Java2D would draw, with some differences:

- **Lines and rectangles** are the same, pixel for pixel, including where Bresenham's algorithm has to choose between two pixels.
- **Filled ovals** are nearly the same, and **outlined ovals** differ by a pixel here and there, because Java2D draws them as Bezier curves.
- **Text** uses the Spectrum's 8 by 8 font, with the baseline under its seventh pixel line, so it is wider than on a desktop. Characters that aren't ASCII are drawn as `?`, as when printing.
- **Colour** belongs to each 8 by 8 cell rather than to each pixel. Drawing sets the ink of the cells it touches, so where two colours share a cell, the later one wins. Drawing in the background colour clears pixels instead, as on a desktop.
- **Coordinates** outside -2048 to 2047 are moved to the nearest of those, so shapes that far off the screen are drawn differently.
- **`paint`** is called once. On a desktop, it is called again whenever the window needs it, so it should draw the same each time; animation is unsupported.

On a real Spectrum, a line across the screen takes about 70 ms, a circle 96 pixels across about 0.35 s outlined or 0.25 s filled, filling the whole screen about 0.4 s, and text about 4 ms a character. `Shapes` takes about 3 seconds in all. The first call of each method from a call site takes another 30 to 50 ms, to look the method up by name.

### Unsupported features

Everything else is unsupported, including other types such as `long` and `float`, `char` output, string operations, arrays, fields, interfaces and other classes of your own, including nested, inner and anonymous classes, and the rest of AWT and Swing, such as `new Color(r, g, b)`, `repaint()` and events. There is no garbage collector, verifier, exception handling or loading of other classes.

Unsupported features stop the program with an error rather than misbehaving:

```
JVM: unsupported opcode 133 at pc 11
JVM: unsupported method java/io/PrintStream.println:(C)V
JVM: not an int: 12x
JVM: not a double: 1,5
```

The Spectrum still shows `0 OK` after an error, because the JVM always returns to BASIC normally.

Limits:

- 256 constant pool entries, and each distinct string takes two
- 32 methods in the class
- About 17 KB for the class file, the heap and the frames of the methods being run together: everything between the end of the JVM and the user-defined graphics. A small recursive method can call itself about 700 levels deep before the program stops with `JVM: stack overflow`, where Java would throw `StackOverflowError`. Each line read takes its length plus 3 bytes, and memory is never freed, so a program that reads many lines eventually stops with `JVM: out of memory`.

## How it works

The tape holds a BASIC loader, the JVM and, on complete tapes, the class:

```basic
10 CLEAR 25599
20 LOAD ""CODE
30 LOAD ""CODE 48021
40 RANDOMIZE USR 25600
```

The address in line 30 is where the JVM ends, so it changes when the JVM does.

| Address | Contents |
|---|---|
| Up to 25599 | BASIC: the loader, the input line and the machine stack, with about 1.8 KB to spare |
| 25600 | JVM, about 22 KB, including its variables |
| After the JVM | Class file |
| After the class file, up to 65367 | Heap, growing up, and the frames of running methods, growing down from 65367 |
| 65368 | User-defined graphics, as usual |

The screen, at 16384, is the canvas, so graphics take no memory beyond their code.

The JVM's address is `JVM_ADDR` in the Makefile.

The JVM parses the constant pool, lists the methods of the class, and interprets the bytecode of `main` directly from the class file in memory. Calling a method of the class gives it a frame below the caller's, holding its local variables, the caller's state and its operand stack. The interpreter is a single loop: a call and a return only change which frame and bytecode it works on, so the depth of calls doesn't use the Z80's stack, which has little room below BASIC. Java's `int` is held in a 32-bit `long`, and a reference in the same slot holds a 16-bit address, with `null` as 0. A `double` takes two slots, as in Java.

The `double` arithmetic is in [`src/double.c`](src/double.c), modelled on [Berkeley SoftFloat](http://www.jhauser.us/arithmetic/SoftFloat.html). The operations, such as addition with its special cases, are in C and work on numbers unpacked into a sign, an exponent and a 64-bit significand. The primitives below them, such as unpacking, packing, rounding and arithmetic on significands, are in Z80 code, because sccz80 has no 64-bit integer type and compiles 32-bit code large and slow: the Z80 code took the JVM from 21 KB to 19 KB. The primitives also have C versions, using `uint64_t`, so that the host test can check the operations. To print a `double`, it scales the number by a power of ten to between 10^14 and 10^15, using 64-bit significands for the powers of ten, and rounds that to an integer, which gives the 15 digits.

Graphics are in [`src/graphics.c`](src/graphics.c). Setting a pixel and setting a horizontal run of pixels, a byte at a time, are Z80 code, along with the inner loops of lines and text and the edge of an oval on each line, which uses an integer square root. The rest, such as clipping and the outline of a rectangle, is C. A native method can't call Java code itself, so `setVisible` asks the interpreter to call `paint` next, with the canvas and the `Graphics` object in place of its own arguments, as if the program had called it there.

Every string has the layout of a `CONSTANT_Utf8` entry. A constant string is its entry in the class file, so it needs no copying, and a line read with `Scanner` is allocated on the heap. `System.out`, `System.in`, `Scanner` and the other runtime objects are one byte that identifies their class; a `Color` is followed by a byte with its Spectrum colour.

The runtime is written in C, in three tables in `src/zxjvm.c`:

| Table | Holds |
|---|---|
| `classes` | Classes that `new` can create |
| `fields` | Static fields, such as `System.out` |
| `natives` | Methods and constructors, with their class, name, descriptor and handler |

The first call from each call site looks the method up by name, and later calls reuse the result. To add a method, write a handler and add a row.

Output goes through the Spectrum ROM's print routine. Input calls the ROM's line editor, set up the way the ROM's `INPUT LINE` does it.

### Compiler pitfalls

The JVM is compiled with sccz80, which miscompiled some valid C in this project. These are worth remembering when changing the C code, especially 32-bit code:

| C code | What went wrong | Used instead |
|---|---|---|
| `0UL - v` | Computes `v - 0` | `~v + 1UL` |
| `v % 10UL` | Divides only the low 16 bits of `v` | Subtracting powers of ten |
| `(signed char)` cast of a byte | Loses the sign | The `s1()` helper |
| `s.a[i]`, for an array `a` in a local struct `s` | Ignores the offset of `a` in the struct | A pointer: `p = &s;` then `p->a[i]` |
| `e <= 0`, for an `int` `e` | Tests the sign with a carry flag left over from the previous instruction | `e < 1` |
| `1UL << n` | Computes `n << 1` | Shifts by a constant, or a variable shifted (`x >> n` works) |
| `P[i]`, for a constant pointer `P` such as `((long *)0xC000)` | Ignores `i` | A pointer variable |
| `v < -2048L`, comparing a `long` with a negative constant | Compares with 63488, losing the sign of the constant's high half | Adding 2048 and comparing as unsigned |
| `jp` or `ret` from the `#asm` body of a `__z88dk_fastcall` function | Not a miscompilation, but a trap: the function pushes its argument on entry, so the return goes to that address | `call` the routine, and let the compiler's code return |

## Tests

```sh
make test               # run all tests
tests/run.sh Edge       # run one test
```

Each test builds a tape and runs it with [`tests/tap2text.py`](tests/tap2text.py), which needs no display. The script uses [SkoolKit](https://skoolkit.ca)'s Z80 simulator to load the tape on an emulated 48K Spectrum, running the ROM's own loader. It then runs the program until BASIC waits for a key, and reads the text from screen memory using the ROM font. At each `scroll?` prompt, it records the screen, adds a `--- key pressed` line and presses ENTER.

When the program reads input, the script types the next line of `tests/<name>.in` one key at a time. Python escapes in that file give other keys, such as `\x0c` for DELETE or `\xc7` for the `<=` keyword. Before pressing ENTER, it records the screen with the typed line on it and adds a `--- typed:` line. A test fails if its program wants more input than the file has, or leaves lines unread.

The test compares the text with a file in `tests/expected/`, and a failure prints a diff. A program that runs for more than 60 seconds of emulated time without reaching a prompt also fails.

A program that uses `java.awt` is also compared with Java2D. When the JVM waits for a key after showing graphics, `tap2text.py --screen` saves the screen memory. [`tests/Render.java`](tests/Render.java) then creates the program's `Canvas` on the desktop's JVM, without running its `main`, and paints it on an image in the same background colour. [`tests/compare_graphics.py`](tests/compare_graphics.py) counts the pixels that only one of them drew, and the cells where the Spectrum's ink differs from the one colour Java drew there. The counts must equal those in `tests/expected/<name>.pixels`, so that any change shows, and an image of the differences is left in `build/tests/<name>-diff.png`.

| Test | What it covers |
|---|---|
| `Fibonacci`, `HelloWorld`, `Adder`, `Loan`, `Shapes` | The example programs |
| `Edge` | Int limits, overflow, negative constants, branches |
| `Arithmetic` | Multiplication, division and remainder with every sign and overflow, divisors over 16 bits, `ineg`, the static runtime methods, `parseInt("")` |
| `Doubles` | Each `double` operation, compared with the same expression on constants, which `javac` evaluates itself; infinities, NaN, `-0.0`, subnormals, conversions, `parseDouble` |
| `PrintDoubles` | Printing doubles with and without an exponent, special values, and the differences from Java listed in [Doubles](#doubles) |
| `Scroll` | 32-bit wraparound, a countdown loop, paging through three screens |
| `Strings` | String output, local variables, `null`, non-ASCII and wrapping |
| `LongString` | The `scroll?` prompt caused only by wrapped text |
| `References` | `null` checks and comparing references |
| `Methods` | Static and instance methods with each type of argument and result, recursion, constructors |
| `Input` | `Scanner` lines and numbers, the echo, DELETE, a keyword, what `nextInt()` leaves for `nextLine()` |
| `InputInts` | Int limits, signs, leading zeros and spaces in input |
| `BadOpcode` | Error for an unsupported opcode |
| `BadMethod` | Error for an unsupported method, `println(char)` |
| `BadInput` | Error for a number out of the range of `int` |
| `StackOverflow` | Error for recursion that never ends |
| `GraphicsLines` | Lines at every slope and rectangles, partly off the screen, `clearRect` and drawing in the background colour; the same pixels as Java2D |
| `GraphicsOvals` | Outlined and filled ovals and circles of many sizes, partly off the screen |
| `GraphicsText` | Text in the character grid, which `tap2text.py` reads back, non-ASCII characters, clipping at the edge, a dark background, a method called from `paint` |
| `BadDouble` | Error for a number with a decimal comma |
| `DivideByZero` | Error for division by zero |
| `NoMain` | Error for a class without `main` |

Before those, `make test` runs two tests of the `double` arithmetic on its own, which `make test-double` also runs:

- [`tests/double/host_test.c`](tests/double/host_test.c) compiles `src/double.c` for the host, with the C versions of the primitives, and compares it with the host's IEEE 754 hardware on special values and a million random operand pairs. The random numbers are chosen so that rounding ties, cancellation, subnormals and overflow all occur. It also compares the 15 digits for printing with the C library's `printf`, which rounds exactly, and reports how many came out the other way because they were almost exactly halfway.
- [`tests/double/z80_test.py`](tests/double/z80_test.py) runs the same code compiled by sccz80, with the Z80 primitives, in SkoolKit's simulator, and compares thousands of results with Python's floats and its formatting of them. This catches miscompilations, which the host can't.

The tests need Python 3 with the `venv` module. `make test` creates a Python environment in `.venv` with the SkoolKit version from `tests/requirements.txt`; you can also create it with `make venv`. To use another interpreter that has SkoolKit installed, set `PYTHON`.

A whole test takes a fraction of a second to run. The tests only cover the 48K Spectrum.

A character cell that matches no character appears as `�` in the text. `build/tests/<name>.png` shows the final screen as an image.

You can also run a single tape yourself:

```sh
.venv/bin/python tests/tap2text.py build/HelloWorld.tap
```

To change what a test expects, edit its text file, or run `tests/run.sh --update Edge` and check the diff before committing it.

## Claude Code skill

[`.claude/skills/zx-spectrum-run`](.claude/skills/zx-spectrum-run/SKILL.md) lets Claude Code run Spectrum programs and see their output. It starts Fuse, loads the program, waits for the screen to settle and saves a screenshot.

You can also run its script directly:

```sh
.claude/skills/zx-spectrum-run/scripts/zx-run.sh --out screen.png build/Fibonacci.tap
```

## Project layout

```
.claude/skills/  Claude Code skill for running programs in Fuse
docs/            Screenshots for this README
examples/        Example Java programs
src/zxjvm.c      The JVM
src/double.c     IEEE 754 double arithmetic
tools/MkTap.java Builds the tape images
tests/           Test programs, runner, headless tape runner and expected output
tests/double/    Tests of the double arithmetic on the host and on the Z80
build/           Generated files
```

## Status

Tested in Fuse on emulated 48K and 128K machines, but not on real Spectrum hardware.
