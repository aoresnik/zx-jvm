import java.util.Scanner;

public class Adder {
    public static void main(String[] args) {
        Scanner in = new Scanner(System.in);

        System.out.print("What is your name? ");
        String name = in.nextLine();
        System.out.print("Hello, ");
        System.out.print(name);
        System.out.println("!");

        System.out.println("Enter numbers to add, 0 to stop.");
        int total = 0;
        int count = 0;
        int largest = Integer.MIN_VALUE;
        while (true) {
            System.out.print("Number: ");
            int n = in.nextInt();
            if (n == 0) {
                break;
            }
            total += n;
            count++;
            if (n > largest) {
                largest = n;
            }
        }

        if (count == 0) {
            System.out.println("No numbers to add.");
        } else {
            System.out.print("Count:   ");
            System.out.println(count);
            System.out.print("Total:   ");
            System.out.println(total);
            System.out.print("Largest: ");
            System.out.println(largest);
        }
        in.close();
    }
}
