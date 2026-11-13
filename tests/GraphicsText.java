import java.awt.Canvas;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import javax.swing.JFrame;

/**
 * Text drawn in the character grid, so that tap2text.py reads it back, by a
 * method of the class called from paint; a dark background.
 */
public class GraphicsText extends Canvas {
    private void label(Graphics g, String s, int column, int row) {
        g.drawString(s, column * 8, row * 8 + 7);   // the baseline is under the seventh pixel line
    }

    public void paint(Graphics g) {
        g.setColor(Color.WHITE);
        label(g, "Text on a canvas", 0, 0);
        g.setColor(Color.YELLOW);
        label(g, "drawString(\"a\", x, y)", 2, 2);
        g.setColor(Color.CYAN);
        label(g, "café €5 ^`", 4, 4);
        label(g, "cut off at the right edge of the screen", 20, 6);
        g.drawString("between rows", 0, 75);
        g.setColor(Color.BLUE);
        label(g, "in the background colour", 0, 12);
    }

    public static void main(String[] args) {
        JFrame frame = new JFrame("GraphicsText");
        GraphicsText canvas = new GraphicsText();
        canvas.setBackground(Color.BLUE);
        canvas.setPreferredSize(new Dimension(256, 192));
        frame.add(canvas);
        frame.pack();
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setVisible(true);
    }
}
