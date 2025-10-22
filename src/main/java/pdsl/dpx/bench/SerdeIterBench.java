package pdsl.dpx.bench;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Random;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

import pdsl.dpx.Options;
import pdsl.dpx.Serde;
import pdsl.dpx.SerdeInputStream;
import pdsl.dpx.SerdeOutputStream;
import pdsl.dpx.type.TypeTraits;

public class SerdeIterBench {
    public static String generateRandomChars(String candidateChars, int length) {
        StringBuilder sb = new StringBuilder();
        Random random = new Random();
        for (int i = 0; i < length; i++) {
            sb.append(candidateChars.charAt(random.nextInt(candidateChars.length())));
        }
        return sb.toString();
    }

    public static void doSerIter() throws FileNotFoundException, IOException {
        File f = new File("/home/lsc/dpx/.test_spill/serde_bench");
        FileOutputStream fos = new FileOutputStream(f);
        BufferedOutputStream bos = new BufferedOutputStream(fos);
        SerdeOutputStream sos = new SerdeOutputStream(bos);
        sos.writeObject(new String("111222333"));
        sos.writeObject(new Integer(111222333));
        sos.flush();
        sos.close();
    }

    static public void doDeIterMul() throws InterruptedException, ExecutionException {
        class DeWorker implements Runnable {
            private int i;

            public DeWorker(int i) {
                this.i = i;
            }

            private void doDeIter() throws FileNotFoundException, IOException {
                ArrayList<String> ls = new ArrayList<>();
                ArrayList<Integer> is = new ArrayList<>();
                File f = new File("/home/lsc/dpx/.test_spill/p" + Integer.toString(i));
                // FileInputStream fos = new FileInputStream(f);
                NioBufferedFileInputStream bos = new NioBufferedFileInputStream(f);
                SerdeInputStream sos = new SerdeInputStream(bos);
                while (true) {
                    try {
                        ls.add(sos.readObject(String.class));
                        is.add(sos.readObject(Integer.class));
                        if (is.size() % 1000 == 0) {
                            System.err.println(is.size());
                        }
                    } catch (EOFException e) {
                        break;
                    } catch (IOException e) {
                        break;
                    }
                }
                sos.close();
            }

            @Override
            public void run() {
                try {
                    doDeIter();
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }

        }
        ExecutorService executor = Executors.newCachedThreadPool();
        LinkedList<Future<?>> fs = new LinkedList<>();
        for (int i = 1; i < 3; i += 2) {
            fs.add(executor.submit(new DeWorker(i)));
        }
        for (Future<?> f : fs) {
            f.get();
        }
        executor.shutdown();
    }

    public static void main(String[] args)
            throws FileNotFoundException, IOException, InterruptedException, ExecutionException {
        Serde.Initialize(Options.defaultOptions);
        Serde.Register(new TypeTraits<char[]>() {
        });
        Serde.Register(new TypeTraits<String>() {
        });
        Serde.Register(new TypeTraits<Integer>() {
        });
        Serde.ShowRegisteredClass();
        // doSerIter();
        doDeIterMul();
        Serde.Destroy();
    }
}
