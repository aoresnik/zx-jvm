import java.awt.Canvas;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import javax.swing.JFrame;

/** Draws lines, rectangles, circles and text, the same on a desktop and on the Spectrum. */
public class Shapes extends Canvas {
    public void paint(Graphics g) {
        g.setColor(Color.BLUE);
        g.drawString("Shapes in Java", 72, 14);

        g.setColor(Color.BLACK);
        g.drawRect(8, 24, 72, 72);
        g.setColor(Color.RED);
        g.fillRect(16, 32, 57, 57);

        g.setColor(Color.BLACK);
        g.drawOval(96, 24, 72, 72);
        g.setColor(Color.GREEN);
        g.fillOval(104, 32, 57, 57);

        g.setColor(Color.MAGENTA);
        for (int i = 0; i <= 72; i += 8) {
            g.drawLine(184, 24 + i, 184 + i, 96);
        }

        g.setColor(Color.BLUE);
        for (int r = 8; r <= 48; r += 8) {
            g.drawOval(128 - r, 144 - r, 2 * r, 2 * r);
        }
        g.setColor(Color.BLACK);
        g.drawLine(0, 191, 255, 104);
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
