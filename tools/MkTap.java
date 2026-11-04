import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

/**
 * Builds a Spectrum tape image holding a BASIC loader, the JVM and optionally a
 * class file. Without one, the loader waits for a key, so that the tape can be
 * changed, and then expects the class as the next CODE block.
 *
 * Usage: java MkTap.java out.tap jvm.bin jvmAddr classAddr [class.class blockName]
 */
public class MkTap {
    private static final int TYPE_PROGRAM = 0;
    private static final int TYPE_CODE = 3;
    private static final int FLAG_HEADER = 0x00;
    private static final int FLAG_DATA = 0xFF;

    private static final int TOKEN_CODE = 0xAF;
    private static final int TOKEN_INKEY = 0xA6;
    private static final int TOKEN_USR = 0xC0;
    private static final int TOKEN_NOT_EQUAL = 0xC9;
    private static final int TOKEN_THEN = 0xCB;
    private static final int TOKEN_GO_TO = 0xEC;
    private static final int TOKEN_LOAD = 0xEF;
    private static final int TOKEN_PAUSE = 0xF2;
    private static final int TOKEN_PRINT = 0xF5;
    private static final int TOKEN_RANDOMIZE = 0xF9;
    private static final int TOKEN_IF = 0xFA;
    private static final int TOKEN_CLEAR = 0xFD;

    private static final int AUTOSTART_LINE = 10;

    public static void main(String[] args) throws IOException {
        if (args.length != 4 && args.length != 6) {
            System.err.println("usage: MkTap out.tap jvm.bin jvmAddr classAddr [class.class blockName]");
            System.exit(2);
        }
        byte[] jvm = Files.readAllBytes(Path.of(args[1]));
        int jvmAddr = Integer.parseInt(args[2]);
        int classAddr = Integer.parseInt(args[3]);

        byte[] basic = loader(jvmAddr, classAddr, args.length == 4);
        if (jvmAddr + jvm.length > classAddr) {
            throw new IOException("JVM overlaps the class file");
        }

        ByteArrayOutputStream tap = new ByteArrayOutputStream();
        writeFile(tap, TYPE_PROGRAM, "zxjvm", basic, AUTOSTART_LINE, basic.length);
        writeFile(tap, TYPE_CODE, "zxjvm", jvm, jvmAddr, 32768);
        if (args.length == 6) {
            byte[] cls = Files.readAllBytes(Path.of(args[4]));
            writeFile(tap, TYPE_CODE, args[5], cls, classAddr, 32768);
        }
        Files.write(Path.of(args[0]), tap.toByteArray());
    }

    /**
     * 10 CLEAR jvmAddr-1 : 20 LOAD "" CODE : 30 LOAD "" CODE classAddr : 40 RANDOMIZE USR jvmAddr
     * with, if promptForClass, 24 PRINT "..." : 26 PAUSE 0 : 28 IF INKEY$<>"" THEN GO TO 28
     * before line 30. LOAD treats a held SPACE as BREAK, hence the wait for release.
     */
    private static byte[] loader(int jvmAddr, int classAddr, boolean promptForClass) {
        ByteArrayOutputStream basic = new ByteArrayOutputStream();

        ByteArrayOutputStream clear = new ByteArrayOutputStream();
        clear.write(TOKEN_CLEAR);
        writeNumber(clear, jvmAddr - 1);
        writeLine(basic, 10, clear);

        byte[] load = {(byte) TOKEN_LOAD, '"', '"', (byte) TOKEN_CODE};
        ByteArrayOutputStream loadJvm = new ByteArrayOutputStream();
        loadJvm.writeBytes(load);
        writeLine(basic, 20, loadJvm);

        if (promptForClass) {
            ByteArrayOutputStream print = new ByteArrayOutputStream();
            print.write(TOKEN_PRINT);
            print.writeBytes("\"Insert class tape, press a key\"".getBytes(StandardCharsets.US_ASCII));
            writeLine(basic, 24, print);

            ByteArrayOutputStream pause = new ByteArrayOutputStream();
            pause.write(TOKEN_PAUSE);
            writeNumber(pause, 0);
            writeLine(basic, 26, pause);

            ByteArrayOutputStream release = new ByteArrayOutputStream();
            release.write(TOKEN_IF);
            release.write(TOKEN_INKEY);
            release.write(TOKEN_NOT_EQUAL);
            release.write('"');
            release.write('"');
            release.write(TOKEN_THEN);
            release.write(TOKEN_GO_TO);
            writeNumber(release, 28);
            writeLine(basic, 28, release);
        }

        ByteArrayOutputStream loadClass = new ByteArrayOutputStream();
        loadClass.writeBytes(load);
        writeNumber(loadClass, classAddr);
        writeLine(basic, 30, loadClass);

        ByteArrayOutputStream run = new ByteArrayOutputStream();
        run.write(TOKEN_RANDOMIZE);
        run.write(TOKEN_USR);
        writeNumber(run, jvmAddr);
        writeLine(basic, 40, run);

        return basic.toByteArray();
    }

    /** A BASIC number: its digits, then the marker and the hidden 5-byte value. */
    private static void writeNumber(ByteArrayOutputStream out, int value) {
        out.writeBytes(Integer.toString(value).getBytes(StandardCharsets.US_ASCII));
        out.write(0x0E);
        out.write(0);
        out.write(0);
        out.write(value & 0xFF);
        out.write(value >> 8);
        out.write(0);
    }

    private static void writeLine(ByteArrayOutputStream out, int number, ByteArrayOutputStream body) {
        out.write(number >> 8);
        out.write(number & 0xFF);
        int length = body.size() + 1;
        out.write(length & 0xFF);
        out.write(length >> 8);
        out.writeBytes(body.toByteArray());
        out.write(0x0D);
    }

    private static void writeFile(ByteArrayOutputStream tap, int type, String name, byte[] data,
                                  int param1, int param2) {
        ByteArrayOutputStream header = new ByteArrayOutputStream();
        header.write(type);
        for (int i = 0; i < 10; i++) {
            header.write(i < name.length() ? name.charAt(i) : ' ');
        }
        writeWord(header, data.length);
        writeWord(header, param1);
        writeWord(header, param2);
        writeBlock(tap, FLAG_HEADER, header.toByteArray());
        writeBlock(tap, FLAG_DATA, data);
    }

    private static void writeBlock(ByteArrayOutputStream tap, int flag, byte[] data) {
        writeWord(tap, data.length + 2);
        tap.write(flag);
        tap.writeBytes(data);
        int checksum = flag;
        for (byte b : data) {
            checksum ^= b & 0xFF;
        }
        tap.write(checksum);
    }

    private static void writeWord(ByteArrayOutputStream out, int value) {
        out.write(value & 0xFF);
        out.write(value >> 8);
    }
}
