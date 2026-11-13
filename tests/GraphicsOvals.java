import java.awt.Canvas;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import javax.swing.JFrame;

/** Circles and ellipses, outlined and filled, of many sizes and partly off the canvas. */
public class GraphicsOvals extends Canvas {
    public void paint(Graphics g) {
        int x = 2;
        for (int d = 0; d <= 13; d++) {
            g.drawOval(x, 2, d, d);
            g.fillOval(x, 20, d, d);
            x += d + 4;
        }
        g.setColor(Color.BLUE);
        g.drawOval(4, 40, 100, 60);
        g.fillOval(14, 50, 80, 40);
        g.setColor(Color.RED);
        g.drawOval(120, 40, 31, 70);
        g.fillOval(160, 40, 30, 71);
        g.drawOval(200, 40, 50, 1);
        g.drawOval(200, 50, 1, 50);
        g.fillOval(210, 60, 2, 30);
        g.setColor(Color.BLACK);
        g.drawOval(-40, 110, 100, 100);         // off the edges
        g.fillOval(200, 150, 100, 80);
        g.drawOval(80, 120, 63, 63);
        g.drawOval(150, 120, 40, -1);           // nothing
        g.fillOval(150, 120, 0, 40);            // nothing
    }

    public static void main(String[] args) {
        JFrame frame = new JFrame("GraphicsOvals");
        GraphicsOvals canvas = new GraphicsOvals();
        canvas.setBackground(Color.WHITE);
        canvas.setPreferredSize(new Dimension(256, 192));
        frame.add(canvas);
        frame.pack();
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setVisible(true);
    }
}
