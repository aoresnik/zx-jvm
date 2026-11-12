public class BadOpcode {
    public static void main(String[] args) {
        int a = 6;
        System.out.println(a);
        long b = a;     // i2l: long is unsupported
        System.out.println(b);
    }
}
