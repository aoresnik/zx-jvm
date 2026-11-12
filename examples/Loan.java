import java.util.Scanner;

/* The monthly payment of a fixed-rate loan, and the balance left after each year. */
public class Loan {
    public static void main(String[] args) {
        Scanner in = new Scanner(System.in);

        // Double.parseDouble reads 4.5 the same way in every locale
        System.out.print("Loan amount: ");
        double principal = Double.parseDouble(in.nextLine());
        System.out.print("Interest, % a year: ");
        double rate = Double.parseDouble(in.nextLine()) / 1200;     // a month, not in %
        System.out.print("Years: ");
        int years = in.nextInt();
        int months = years * 12;

        // How much a balance grows in a year, and over the whole loan
        double yearGrowth = 1;
        for (int month = 0; month < 12; month++) {
            yearGrowth *= 1 + rate;
        }
        double growth = 1;
        for (int year = 0; year < years; year++) {
            growth *= yearGrowth;
        }

        double payment;
        double yearPayments;    // what a year of payments is worth at its end
        if (rate == 0) {
            payment = principal / months;
            yearPayments = 12;
        } else {
            payment = principal * rate * growth / (growth - 1);
            yearPayments = (yearGrowth - 1) / rate;
        }

        // Amounts are printed in whole cents, as printing a double is unsupported
        int cents = (int) (payment * 100 + 0.5);
        System.out.print("Monthly payment: ");
        System.out.print(cents / 100);
        System.out.print(cents % 100 < 10 ? ".0" : ".");
        System.out.println(cents % 100);

        cents = (int) ((payment * months - principal) * 100 + 0.5);
        System.out.print("Total interest: ");
        System.out.print(cents / 100);
        System.out.print(cents % 100 < 10 ? ".0" : ".");
        System.out.println(cents % 100);

        System.out.println();
        System.out.println("Year  Balance");
        double balance = principal;
        for (int year = 1; year <= years; year++) {
            balance = balance * yearGrowth - payment * yearPayments;
            cents = (int) (balance * 100 + 0.5);
            System.out.print(year < 10 ? "   " : "  ");
            System.out.print(year);
            System.out.print("  ");
            System.out.print(cents / 100);
            System.out.print(cents % 100 < 10 ? ".0" : ".");
            System.out.println(cents % 100);
        }
    }
}
