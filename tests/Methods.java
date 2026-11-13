public class Methods {
    static int square(int x) {
        return x * x;
    }

    static int add3(int a, int b, int c) {
        return a + b + c;
    }

    static double half(double d) {
        return d / 2;
    }

    static double mix(int a, double d, int b) {
        return a + d * b;
    }

    static String pick(boolean first, String a, String b) {
        return first ? a : b;
    }

    static int fib(int n) {
        return n < 2 ? n : fib(n - 1) + fib(n - 2);
    }

    static int depth(int n) {
        return n == 0 ? 0 : 1 + depth(n - 1);
    }

    static void greet(String name) {
        System.out.print("Hello, ");
        System.out.println(name);
    }

    static void countdown(int n) {
        if (n == 0) {
            System.out.println("go");
            return;
        }
        System.out.print(n);
        System.out.print(" ");
        countdown(n - 1);
    }

    int twice(int x) {
        return x + x;
    }

    Methods self() {
        return this;
    }

    void show(int x, double d) {
        System.out.print(twice(x));
        System.out.print(" ");
        System.out.println(d);
    }

    public static void main(String[] args) {
        System.out.println(square(12));
        System.out.println(add3(1, 20, 300));
        System.out.println(half(5));
        System.out.println(mix(1, 2.5, 4));
        System.out.println(pick(true, "first", "second"));
        System.out.println(pick(false, "first", "second"));
        System.out.println(fib(15));
        System.out.println(depth(200));
        greet("Spectrum");
        countdown(5);

        Methods m = new Methods();
        System.out.println(m.twice(21));
        m.self().show(4, 0.5);
        if (m.self() == m)
            System.out.println("same object");
        int local = 7;
        double kept = 1.25;
        square(3);
        System.out.print(local);
        System.out.print(" ");
        System.out.println(kept);
    }
}
