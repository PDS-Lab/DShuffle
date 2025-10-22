package pdsl.dpx.bench;

import java.util.LinkedList;
import java.util.Random;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

import pdsl.dpx.PipelineTransEnv;

public class PipelineSpill {

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
                byte[] array = new byte[1024];
                for (int i = 0; i < 32 * (1024 - 1); i++) {
                    r.nextBytes(array);
                    PipelineTransEnv.Append(i % 32, array, array, false);
                }
                if (id == 15) {
                    r.nextBytes(array);
                    for (int i = 0; i < 32; i++) {
                        PipelineTransEnv.Append(i, array, array, true);
                    }
                }
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
        PipelineTransEnv.Initialize(args[0], args[1]);
        PipelineTransEnv.TriggerSpillStart();
        doSpill();
        PipelineTransEnv.WaitForSpillDone();
        PipelineTransEnv.Destroy();
    }
}
