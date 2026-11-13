public class PrintDoubles {
    public static void main(String[] args) {
        // The JVM prints at most 15 significant digits; where Java prints
        // more, the comment says what it prints.
        double third = 1;
        third /= 3;
        double zero = 0;

        System.out.println(0.1);
        System.out.println(-2.5);
        System.out.println(100.0);
        System.out.println(1234567.125);
        System.out.println(1e7);
        System.out.println(-12345678.9);
        System.out.println(0.001);
        System.out.println(0.000999);
        System.out.println(1e-5);
        System.out.println(6.02214076e23);
        System.out.println(2147483647.0);
        System.out.println(zero);
        System.out.println(-zero);
        System.out.println(zero / zero);
        System.out.println(-1 / zero);
        System.out.println(third);              // 0.3333333333333333
        System.out.println(0.1 + 0.2);          // 0.30000000000000004
        System.out.println(Double.MAX_VALUE);   // 1.7976931348623157E308
        System.out.println(Double.MIN_VALUE);   // 4.9E-324
        System.out.print(Double.toString(9.5));
        System.out.print(" ");
        System.out.println(String.valueOf(third * 2));  // 0.6666666666666666
    }
}
