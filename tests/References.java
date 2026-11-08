public class References {
    public static void main(String[] args) {
        String a = "same";
        String b = "same";
        String c = "other";
        String n = null;

        if (n == null) System.out.println("n == null"); else System.out.println("FAIL 1");
        if (n != null) System.out.println("FAIL 2"); else System.out.println("n is null");
        if (a != null) System.out.println("a != null"); else System.out.println("FAIL 3");
        if (a == null) System.out.println("FAIL 4"); else System.out.println("a is not null");
        if (args == null) System.out.println("args == null"); else System.out.println("FAIL 5");

        // String literals are interned, so equal literals are the same object
        if (a == b) System.out.println("a == b"); else System.out.println("FAIL 6");
        if (a != b) System.out.println("FAIL 7"); else System.out.println("a is b");
        if (a != c) System.out.println("a != c"); else System.out.println("FAIL 8");
        if (a == c) System.out.println("FAIL 9"); else System.out.println("a is not c");
        if (System.out == System.out) System.out.println("out == out"); else System.out.println("FAIL 10");

        int count = 0;
        String s = "x";
        while (s != null) {
            count++;
            if (count == 3)
                s = null;
        }
        System.out.print("loop ran ");
        System.out.println(count);
    }
}
