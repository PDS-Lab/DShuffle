package pdsl.dpx.bench;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Random;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

import pdsl.dpx.NaiveTransEnv;

public class NaiveSpill {
    static public void doSpill() throws InterruptedException, ExecutionException {
        class Spiller implements Runnable {
            int id;

            Spiller(int id) {
                this.id = id;
            }

            @Override
            public void run() {
                System.err.printf("run %d\n", id);
                Random r = new Random();
                ByteArrayOutputStream o = new ByteArrayOutputStream();
                ArrayList<Integer> offsets = new ArrayList<>();
                ArrayList<Integer> hashcodes = new ArrayList<>();
                try {
                    for (int i = 0; i < 32 * 1024; i++) {
                        byte[] array = new byte[1024];
                        r.nextBytes(array);
                        offsets.add(o.size());
                        o.write(array);
                        hashcodes.add(array.hashCode() % 32);
                    }
                    offsets.add(o.size());
                } catch (IOException e) {
                    e.printStackTrace();
                }
                NaiveTransEnv.Spill(o.toByteArray());
                System.err.printf("stop %d\n", id);
            }

        }
        ExecutorService executor = Executors.newCachedThreadPool();
        LinkedList<Future<?>> fs = new LinkedList<>();
        for (int i = 0; i < 16; i++) {
            fs.add(executor.submit(new Spiller(i)));
        }
        for (Future<?> f : fs) {
            f.get();
        }
        executor.shutdown();
    }

    static public void main(String[] args) throws InterruptedException, ExecutionException {
        // s21 "0000:43:00.1", "/home/lsc/dpx/.test_spill", "/dev/nvme2n1p1"
        // s20 "0000:99:00.1", "/home/lsc/dpx/.test_spill", "/dev/nvme3n1p1"
        NaiveTransEnv.Initialize(args[0], args[1]);
        doSpill();
        Thread.sleep(10000);
        NaiveTransEnv.Destroy();
    }
}
