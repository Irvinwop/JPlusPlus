public class GnuRandomThreadDriver {
    static volatile Throwable failure;
    public static void main(String[] args) throws Exception {
        for (int seed : new int[] {0, Integer.MIN_VALUE, -1}) {
            Main.seed_rng(seed);
            Thread[] workers = new Thread[8];
            for (int i = 0; i < workers.length; ++i) {
                workers[i] = new Thread(() -> {
                    try {
                        for (int step = 0; step < 100003; ++step)
                            Main.read_rng();
                    } catch (Throwable error) {
                        failure = error;
                    }
                });
                workers[i].start();
            }
            for (Thread worker : workers)
                worker.join();
            if (failure != null)
                throw new AssertionError(failure);
            for (int i = 0; i < 64; ++i)
                System.out.println(Main.read_rng());
        }
    }
}
