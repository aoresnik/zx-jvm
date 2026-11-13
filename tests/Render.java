import java.awt.Canvas;
import java.awt.Color;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.image.BufferedImage;

/**
 * Paints a test program's Canvas with the desktop's Java2D, for
 * compare_graphics.py to compare with the Spectrum's screen. Its main, which
 * opens a window, is not run: the component is created with its constructor
 * and painted on an image in the program's background colour.
 *
 * Prints 192 lines of 256 characters: '.' for the background, otherwise the
 * Spectrum colour number (0 to 7) of the colour drawn, or '?' for another.
 *
 * Usage: java -cp classes -Djava.awt.headless=true Render.java ClassName paperNumber
 */
public class Render {
    /** The colours of java.awt.Color that the Spectrum has, in the order of its colour numbers */
    private static final Color[] COLORS = {
        Color.BLACK, Color.BLUE, Color.RED, Color.MAGENTA,
        Color.GREEN, Color.CYAN, Color.YELLOW, Color.WHITE
    };

    public static void main(String[] args) throws Exception {
        Canvas canvas = (Canvas) Class.forName(args[0]).getDeclaredConstructor().newInstance();
        Color paper = COLORS[Integer.parseInt(args[1])];

        BufferedImage image = new BufferedImage(256, 192, BufferedImage.TYPE_INT_RGB);
        Graphics g = image.getGraphics();
        g.setColor(paper);
        g.fillRect(0, 0, 256, 192);
        ((Graphics2D) g).setBackground(paper);    // for clearRect, as a window's Graphics has
        g.setColor(Color.BLACK);
        canvas.paint(g);
        g.dispose();

        StringBuilder out = new StringBuilder();
        for (int y = 0; y < 192; y++) {
            for (int x = 0; x < 256; x++) {
                int rgb = image.getRGB(x, y) & 0xFFFFFF;
                char c = '?';
                if (rgb == (paper.getRGB() & 0xFFFFFF)) {
                    c = '.';
                } else {
                    for (int i = 0; i < COLORS.length; i++) {
                        if (rgb == (COLORS[i].getRGB() & 0xFFFFFF)) {
                            c = (char) ('0' + i);
                        }
                    }
                }
                out.append(c);
            }
            out.append('\n');
        }
        System.out.print(out);
    }
}
