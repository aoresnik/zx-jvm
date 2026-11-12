public class BadDouble {
    public static void main(String[] args) {
        System.out.println((int) (Double.parseDouble("1.5") * 2));
        System.out.println((int) Double.parseDouble("1,5"));    // a decimal comma
    }
}
