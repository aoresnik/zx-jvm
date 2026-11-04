public class Scroll {
    public static void main(String[] args) {
        int a = 0, b = 1;
        for (int i = 0; i < 50; i++) {
            System.out.println(a);
            int next = a + b;
            a = b;
            b = next;
        }
        for (int i = 3; i > 0; i--) System.out.println(i);
    }
}
