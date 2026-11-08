public class Strings {
    public static void main(String[] args) {
        String greeting = "Hello, Spectrum!";
        System.out.println(greeting);
        System.out.print("x = ");
        System.out.print(42);
        System.out.println();
        System.out.print("n = ");
        System.out.println(-7);
        System.out.println("folded " + "by javac");
        System.out.println("two\nlines");
        System.out.println("");
        System.out.println("non-ASCII: café €5 😀 tab\tend");
        System.out.println("ROM differs: ^ ` \u007f");
        String nothing = null;
        System.out.println(nothing);
        System.out.println("A line wider than the 32 columns of the screen wraps around.");
        for (int i = 0; i < 3; i++) {
            String s = "loop ";
            System.out.print(s);
            System.out.print(i);
            System.out.print(" ");
        }
        System.out.println();
    }
}
