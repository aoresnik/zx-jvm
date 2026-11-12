public class Arithmetic {
    public static void main(String[] args) {
        // Locals, not constants, so javac doesn't compute the results itself
        int seven = 7;
        int minus7 = -7;
        int two = 2;
        int minus2 = -2;
        int big = 123456789;
        int min = Integer.MIN_VALUE;
        int max = Integer.MAX_VALUE;
        int minus1 = -1;

        System.out.println("imul");
        System.out.println(seven * minus7);
        System.out.println(big * 1000);         // wraps around
        System.out.println(65536 * big);        // only the low 16 bits remain
        System.out.println(max * max);
        System.out.println(min * minus1);       // wraps to itself

        System.out.println("idiv, irem");
        System.out.print(seven / two);
        System.out.print(" ");
        System.out.println(seven % two);
        System.out.print(minus7 / two);
        System.out.print(" ");
        System.out.println(minus7 % two);       // takes the sign of the dividend
        System.out.print(seven / minus2);
        System.out.print(" ");
        System.out.println(seven % minus2);
        System.out.print(minus7 / minus2);
        System.out.print(" ");
        System.out.println(minus7 % minus2);
        System.out.print(max / 65537);          // divisors over 16 bits too
        System.out.print(" ");
        System.out.println(max % 65537);
        System.out.print(big / 1000000);
        System.out.print(" ");
        System.out.println(big % 1000000);
        System.out.print(min / minus1);         // wraps to itself
        System.out.print(" ");
        System.out.println(min % minus1);
        System.out.print(min / 10);
        System.out.print(" ");
        System.out.println(min % 10);

        System.out.println("ineg");
        System.out.print(-seven);
        System.out.print(" ");
        System.out.print(-minus7);
        System.out.print(" ");
        System.out.println(-min);               // wraps to itself

        System.out.println("invokestatic");
        System.out.print(Math.abs(minus7));
        System.out.print(" ");
        System.out.println(Math.abs(min));      // stays negative
        System.out.print(Math.max(minus7, two));
        System.out.print(" ");
        System.out.println(Math.min(minus7, two));
        System.out.print(Integer.parseInt("-2147483648"));
        System.out.print(" ");
        System.out.println(Integer.parseInt("+42") * two);

        System.out.println(Integer.parseInt(""));
    }
}
