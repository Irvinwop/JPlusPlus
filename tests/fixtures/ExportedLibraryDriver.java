import java.util.Scanner;

public class ExportedLibraryDriver {
    public static void main(String[] args) {
        Scanner input = new Scanner(System.in);
        int count = input.nextInt();
        System.out.println(Main.checkpoint());
        while (count-- > 0) {
            int first = input.nextInt();
            int second = input.nextInt();
            System.out.println(Main.evaluate(first, second) + ":" + Main.checkpoint());
        }
    }
}
