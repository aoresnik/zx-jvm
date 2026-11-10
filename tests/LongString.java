public class LongString {
    public static void main(String[] args) {
        // 26 screen lines with no newline: wrapping alone must stop at scroll?
        for (int i = 0; i < 52; i++) {
            System.out.print("0123456789abcdef");
        }
        System.out.println("end");
    }
}
