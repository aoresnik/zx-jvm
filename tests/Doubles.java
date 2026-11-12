public class Doubles {
    public static void main(String[] args) {
        // Each check compares the JVM's result with the same expression on
        // constants, which javac computes itself. + passes, X fails.
        double a = 0.1;
        double b = 0.2;
        double three = 3;
        double big = 1e308;
        double tiny = Double.MIN_VALUE;
        double smallest = Double.MIN_NORMAL;
        double zero = 0;
        double minusZero = -0.0;
        double inf = big * 10;
        double nan = zero / zero;
        int seven = 7;
        int min = Integer.MIN_VALUE;

        System.out.print("arith    ");
        System.out.print(a + b == 0.1 + 0.2 ? "+" : "X");
        System.out.print(a - b == 0.1 - 0.2 ? "+" : "X");
        System.out.print(a * b == 0.1 * 0.2 ? "+" : "X");
        System.out.print(a / b == 0.1 / 0.2 ? "+" : "X");
        System.out.print(b / a == 0.2 / 0.1 ? "+" : "X");
        System.out.print(1 / three == 1.0 / 3 ? "+" : "X");
        System.out.print(-a == -0.1 ? "+" : "X");
        System.out.print(a * three - b == 0.1 * 3 - 0.2 ? "+" : "X");
        System.out.println(1e15 + a == 1e15 + 0.1 ? "+" : "X");

        System.out.print("limits   ");
        System.out.print(inf == Double.POSITIVE_INFINITY ? "+" : "X");
        System.out.print(-big * 10 == Double.NEGATIVE_INFINITY ? "+" : "X");
        System.out.print(big * 1.5 / 2 == 1e308 * 1.5 / 2 ? "+" : "X");
        System.out.print(tiny / 2 == 0 ? "+" : "X");          // ties to even
        System.out.print(tiny * 3 / 2 == Double.MIN_VALUE * 3 / 2 ? "+" : "X");
        System.out.print(smallest / 3 == Double.MIN_NORMAL / 3 ? "+" : "X");
        System.out.print(smallest - tiny == Double.MIN_NORMAL - Double.MIN_VALUE ? "+" : "X");
        System.out.println(1 / big / big == 1 / 1e308 / 1e308 ? "+" : "X");

        System.out.print("special  ");
        System.out.print(nan != nan ? "+" : "X");
        System.out.print(!(nan < 1) && !(nan > 1) && !(nan == 1) ? "+" : "X");
        System.out.print(inf - inf != inf - inf ? "+" : "X");
        System.out.print(zero * inf != zero * inf ? "+" : "X");
        System.out.print(zero == minusZero ? "+" : "X");
        System.out.print(1 / minusZero == Double.NEGATIVE_INFINITY ? "+" : "X");
        System.out.print(1 / (zero * -1) == Double.NEGATIVE_INFINITY ? "+" : "X");
        System.out.print(1 / (minusZero + zero) == Double.POSITIVE_INFINITY ? "+" : "X");
        System.out.println(1 / (a - a) == Double.POSITIVE_INFINITY ? "+" : "X");

        System.out.print("compare  ");
        System.out.print(a < b ? "+" : "X");
        System.out.print(b > a ? "+" : "X");
        System.out.print(a <= a && a >= a ? "+" : "X");
        System.out.print(-a < a ? "+" : "X");
        System.out.print(!(minusZero < zero) ? "+" : "X");
        System.out.print(-inf < -big ? "+" : "X");
        System.out.println(tiny > zero ? "+" : "X");

        System.out.print("convert  ");
        System.out.print((double) min == -2147483648.0 ? "+" : "X");
        System.out.print((double) seven / 2 == 3.5 ? "+" : "X");
        double x;
        double y;
        x = y = 1.5;                                        // dup2
        System.out.println(x + y == 3 ? "+" : "X");
        System.out.print((int) (three + 0.99));
        System.out.print(" ");
        System.out.print((int) -(three + 0.99));
        System.out.print(" ");
        System.out.print((int) (big));
        System.out.print(" ");
        System.out.print((int) -inf);
        System.out.print(" ");
        System.out.println((int) nan);

        System.out.print("parse    ");
        System.out.print(Double.parseDouble("0.1") == 0.1 ? "+" : "X");
        System.out.print(Double.parseDouble("1e-5") == 1e-5 ? "+" : "X");
        System.out.print(Double.parseDouble("-2.5E3") == -2500 ? "+" : "X");
        System.out.print(Double.parseDouble("  42  ") == 42 ? "+" : "X");
        System.out.print(Double.parseDouble("007.50d") == 7.5 ? "+" : "X");
        System.out.print(Double.parseDouble(".5") + Double.parseDouble("5.") == 5.5 ? "+" : "X");
        System.out.print(Double.parseDouble("123456789012345") == 123456789012345.0 ? "+" : "X");
        System.out.print(Double.parseDouble("1e22") == 1e22 ? "+" : "X");
        System.out.print(Double.parseDouble("0.000000000000000000001") == 1e-21 ? "+" : "X");
        System.out.print(Double.parseDouble("100000000000000000000000") == 1e23 ? "+" : "X");
        System.out.print(Double.parseDouble("3.14159265358979") == 3.14159265358979 ? "+" : "X");
        System.out.print(1 / Double.parseDouble("-0") == Double.NEGATIVE_INFINITY ? "+" : "X");
        System.out.print(Double.parseDouble("-Infinity") == -inf ? "+" : "X");
        nan = Double.parseDouble("NaN");
        System.out.print(nan != nan ? "+" : "X");
        Double.parseDouble("1");                            // pop2
        System.out.println();

        System.out.println(Double.parseDouble("0.12345678901234567"));
    }
}
