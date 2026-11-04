public class Edge {
    public static void main(String[] args) {
        System.out.println(2147483647);
        System.out.println(-2147483648);
        int max = 2147483647;
        max = max + 1;
        System.out.println(max);
        int min = -2147483648;
        min = min - 1;
        System.out.println(min);
        System.out.println(-1);
        System.out.println(-5);
        System.out.println(-129);
        System.out.println(1000);
        System.out.println(-1000);
        System.out.println(100000);
        System.out.println(0);
        int z = 0, n = -7, p = 7;
        if (z == 0) System.out.println(1);
        if (p != 0) System.out.println(2);
        if (n < 0) System.out.println(3);
        if (p >= 0) System.out.println(4);
        if (p > 0) System.out.println(5);
        if (n <= 0) System.out.println(6);
        if (n == p) System.out.println(-100);
        if (n > p) System.out.println(-101);
        if (p < n) System.out.println(-102);
        if (n != p) System.out.println(7);
        if (n <= p) System.out.println(8);
        if (p >= n) System.out.println(9);
    }
}
