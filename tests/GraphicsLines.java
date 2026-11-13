import java.awt.Canvas;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics;
import javax.swing.JFrame;

/** Lines in every direction and rectangles, partly off the canvas; drawn as Java2D draws them. */
public class GraphicsLines extends Canvas {
    public void paint(Graphics g) {
        // A star of lines from one point, at every slope
        for (int i = 0; i <= 40; i += 3) {
            g.drawLine(48, 48, 8 + i, 8);
            g.drawLine(48, 48, 88, 8 + i);
            g.drawLine(48, 48, 88 - i, 88);
            g.drawLine(48, 48, 8, 88 - i);
        }
        g.setColor(Color.RED);
        g.drawLine(100, 10, 100, 10);           // a point
        g.drawLine(100, 20, 180, 20);
        g.drawLine(110, 90, 110, 30);
        g.drawLine(-50, 100, 300, 140);         // across the canvas
        g.drawLine(250, -40, 200, 230);

        g.setColor(Color.BLUE);
        g.drawRect(120, 30, 40, 20);
        g.fillRect(170, 30, 30, 20);
        g.drawRect(210, 30, 0, 10);             // a line
        g.drawRect(220, 30, 5, 0);
        g.fillRect(230, 30, 0, 10);             // nothing
        g.drawRect(240, 40, -5, 10);            // nothing
        g.fillRect(-10, 150, 30, 60);           // off the corner
        g.drawRect(230, 170, 40, 40);

        g.setColor(Color.MAGENTA);
        g.setColor(null);                       // ignored, as in Java
        g.fillRect(40, 120, 60, 40);
        g.clearRect(50, 130, 20, 10);           // back to the background
        g.setColor(Color.WHITE);
        g.drawLine(40, 155, 99, 125);           // in the background colour
    }

    public static void main(String[] args) {
        JFrame frame = new JFrame("GraphicsLines");
        GraphicsLines canvas = new GraphicsLines();
        canvas.setBackground(Color.WHITE);
        canvas.setPreferredSize(new Dimension(256, 192));
        frame.add(canvas);
        frame.pack();
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setVisible(true);
    }
}
