package pdsl.dpx.bench;

import java.util.LinkedList;
import java.util.Random;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import pdsl.dpx.Serde;
import pdsl.dpx.TransEnv;
import pdsl.dpx.type.TypeTraits;

public class Spill {

    static public String generateRandomChars(String candidateChars, int length) {
        StringBuilder sb = new StringBuilder();
        Random random = new Random();
        for (int i = 0; i < length; i++) {
            sb.append(candidateChars.charAt(random.nextInt(candidateChars.length())));
        }
        return sb.toString();
    }

    static public String generateRandomASCII(int length) {
        return generateRandomChars("abcdefghijklmnopqrstuvwxyz", length);
    }

    static public String generateRandomASCII(Random r, int mxl, int mnl) {
        if (mxl == mnl) {
            return generateRandomASCII(mnl);
        }
        int m = mxl - mnl;
        return generateRandomASCII(r.nextInt(m) + mnl);
    }

    static public void doSpill(int nthread, int n, int mxl, int mnl)
            throws InterruptedException, ExecutionException {
        class Spiller implements Runnable {
            int id;
            int n;
            int mxl;
            int m;
            int mnl;
            int partition = 32;

            Spiller(int id, int n, int mxl, int mnl) {
                this.id = id;
                this.n = n;
                this.mxl = mxl;
                this.mnl = mnl;
                this.m = mxl - mnl;
            }

            @Override
            public void run() {
                System.err.printf("run %d %d\n", id, n);
                int cur = 0;
                Random r = new Random();
                Serde se = new Serde();
                String k = generateRandomASCII(r, mxl, mnl);
                String v = generateRandomASCII(r, mxl, mnl);
                for (int i = 0; i < n; i++) {
                    int p = k.hashCode() % partition;
                    if (p < 0) {
                        p += partition;
                    }
                    TransEnv.Append(i % partition, se.Serialize(k), se.Serialize(v), false);
                    cur += 1;
                    if (cur % 1000000 == 0) {
                        System.err.println(cur);
                    }
                }
                System.err.printf("stop %d\n", id);
            }

        }
        ExecutorService executor = Executors.newCachedThreadPool();
        LinkedList<Future<?>> fs = new LinkedList<>();
        long start = System.currentTimeMillis();
        for (int i = 0; i < nthread; i++) {
            fs.add(executor.submit(new Spiller(i, n, mxl, mnl)));
        }
        for (Future<?> f : fs) {
            f.get();
        }
        long end = System.currentTimeMillis();

        System.err.printf("%d ms\n", end - start);
        executor.shutdown();
    }

    static public void main(String[] args) throws InterruptedException, ExecutionException {
        // s21 "0000:43:00.1", "/home/lsc/dpx/.test_spill", "/dev/nvme2n1p1"
        // s20 "0000:99:00.1", "/home/lsc/dpx/.test_spill", "/dev/nvme3n1p1"
        Serde.Initialize();
        Serde.Register(new TypeTraits<char[]>() {});
        Serde.Register(new TypeTraits<String>() {});
        Serde.Register(new TypeTraits<Integer>() {});
        TransEnv.Initialize(args[0], args[1], args[2]);
        TransEnv.TriggerSpillStart();
        doSpill(Integer.parseInt(args[3]), Integer.parseInt(args[4]), Integer.parseInt(args[5]),
                Integer.parseInt(args[6]));
        Thread.sleep(60000);
        TransEnv.WaitForSpillDone();
        TransEnv.Destroy();
        Serde.Destroy();
    }
}
