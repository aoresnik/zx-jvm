public class StackOverflow {
    static int depth(int n) {
        return 1 + depth(n + 1);
    }

    public static void main(String[] args) {
        System.out.println("Recursing forever");
        depth(0);
    }
}
