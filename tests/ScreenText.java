import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;
import javax.imageio.ImageIO;

/**
 * Prints the text on a Spectrum screen, read from a screenshot of an emulator
 * window by matching each 8x8 character cell against the font in the 48K ROM.
 *
 * The emulated display is found as the largest band of the border colour, and
 * a pixel counts as ink when it differs from that colour, so the paper must be
 * the same colour as the border (as it is by default). Cells that match no
 * character, in normal or inverse video, are printed as U+FFFD.
 *
 * Usage: java ScreenText.java screen.png 48.rom
 */
public class ScreenText {
    private static final int FONT_ADDR = 0x3D00;    /* characters 32 to 127 */
    private static final int COLUMNS = 32;
    private static final int ROWS = 24;
    /* Fuse shows a 320x240 view: a 32 pixel border each side, 24 above and below */
    private static final int VIEW_WIDTH = 320;
    private static final int VIEW_HEIGHT = 240;
    private static final int BORDER_X = 32;
    private static final int BORDER_Y = 24;

    public static void main(String[] args) throws IOException {
        if (args.length != 2) {
            System.err.println("usage: ScreenText screen.png 48.rom");
            System.exit(2);
        }
        BufferedImage image = ImageIO.read(new File(args[0]));
        if (image == null) {
            throw new IOException("not an image: " + args[0]);
        }
        Map<Long, Character> font = loadFont(Files.readAllBytes(Path.of(args[1])));
        PrintStream out = new PrintStream(System.out, true, StandardCharsets.UTF_8);
        out.println(decode(image, font));
    }

    /** Maps each glyph, as its 8 rows packed into a long, to the character it shows. */
    private static Map<Long, Character> loadFont(byte[] rom) {
        Map<Long, Character> font = new HashMap<>();
        for (int code = 32; code < 128; code++) {
            long glyph = 0;
            for (int y = 0; y < 8; y++) {
                glyph = glyph << 8 | (rom[FONT_ADDR + (code - 32) * 8 + y] & 0xFF);
            }
            char c = switch (code) {
                case 0x5E -> '↑';  /* up arrow */
                case 0x60 -> '£';  /* pound */
                case 0x7F -> '©';  /* copyright */
                default -> (char) code;
            };
            font.put(glyph, c);
            font.putIfAbsent(~glyph, c);
        }
        return font;
    }

    private static String decode(BufferedImage image, Map<Long, Character> font) throws IOException {
        int border = mostCommonColour(image);
        int top = -1;
        int bottom = -1;
        for (int y = 0; y < image.getHeight(); y++) {
            if (isBorderLine(image, border, y)) {
                if (top < 0) {
                    top = y;
                }
                bottom = y;
            }
        }
        int width = image.getWidth();
        int height = bottom - top + 1;
        if (top < 0 || width % VIEW_WIDTH != 0 || height != width / VIEW_WIDTH * VIEW_HEIGHT) {
            throw new IOException("cannot find a " + VIEW_WIDTH + "x" + VIEW_HEIGHT + " display: found "
                + width + "x" + height + " at line " + top);
        }
        int scale = width / VIEW_WIDTH;

        StringBuilder text = new StringBuilder();
        for (int row = 0; row < ROWS; row++) {
            StringBuilder line = new StringBuilder();
            for (int col = 0; col < COLUMNS; col++) {
                long glyph = 0;
                for (int y = 0; y < 8; y++) {
                    for (int x = 0; x < 8; x++) {
                        /* the middle of each scaled-up Spectrum pixel */
                        int px = (BORDER_X + col * 8 + x) * scale + scale / 2;
                        int py = top + (BORDER_Y + row * 8 + y) * scale + scale / 2;
                        boolean ink = (image.getRGB(px, py) & 0xFFFFFF) != border;
                        glyph = glyph << 1 | (ink ? 1 : 0);
                    }
                }
                line.append(font.getOrDefault(glyph, '�'));
            }
            text.append(line.toString().stripTrailing()).append('\n');
        }
        return text.toString().stripTrailing();
    }

    private static int mostCommonColour(BufferedImage image) {
        Map<Integer, Integer> counts = new HashMap<>();
        for (int y = 0; y < image.getHeight(); y++) {
            for (int x = 0; x < image.getWidth(); x++) {
                counts.merge(image.getRGB(x, y) & 0xFFFFFF, 1, Integer::sum);
            }
        }
        return counts.entrySet().stream().max(Map.Entry.comparingByValue()).get().getKey();
    }

    /** A line of the display, rather than of the window's menu or status bar? */
    private static boolean isBorderLine(BufferedImage image, int border, int y) {
        int matching = 0;
        for (int x = 0; x < image.getWidth(); x++) {
            if ((image.getRGB(x, y) & 0xFFFFFF) == border) {
                matching++;
            }
        }
        return matching * 10 >= image.getWidth() * 9;
    }
}
