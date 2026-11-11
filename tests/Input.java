import java.util.Scanner;

public class Input {
    public static void main(String[] args) {
        Scanner in = new Scanner(System.in);
        System.out.print("Name: ");
        String name = in.nextLine();
        System.out.print("Hello, ");
        System.out.println(name);

        System.out.print("Two numbers: ");
        int a = in.nextInt();
        int b = in.nextInt();
        System.out.print("Sum: ");
        System.out.println(a + b);
        // nextInt leaves the rest of the line for nextLine, as on any JVM
        String rest = in.nextLine();
        System.out.print("Rest: [");
        System.out.print(rest);
        System.out.println("]");

        // An empty line is skipped while looking for a number
        System.out.print("Number: ");
        int c = in.nextInt();
        System.out.println(c - 1);

        System.out.print("Line: ");
        in.nextLine();
        System.out.print("Line: ");
        System.out.println(in.nextLine());
        in.close();
    }
}
