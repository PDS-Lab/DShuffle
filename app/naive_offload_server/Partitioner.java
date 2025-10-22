import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

public class Partitioner {
    static public byte[][] doPartition(byte[] src, int n) throws InterruptedException, IOException {
        ObjectInputStream objIn = new ObjectInputStream(new ByteArrayInputStream(src));
        byte[][] results = new byte[n][];
        ByteArrayOutputStream[] bss = new ByteArrayOutputStream[n];
        ObjectOutputStream[] oss = new ObjectOutputStream[n];
        int[] counter = new int[n];
        long s = System.currentTimeMillis();
        Thread t = new Thread(new Runnable() {
            public void run() {
                try {

                    for (int i = 0; i < n; i++) {
                        bss[i] = new ByteArrayOutputStream(4 * 1024 * 1024);
                        oss[i] = new ObjectOutputStream(new BufferedOutputStream(bss[i]));
                        counter[i] = 0;
                    }
                    while (true) {
                        try {
                            Object k = objIn.readObject();
                            Object v = objIn.readObject();

                            int pid = k.hashCode() % n;
                            if (pid < 0) {
                                pid += n;
                            }

                            // System.err.printf("read kv pid: %d\n", pid);

                            oss[pid].writeObject(k);
                            oss[pid].writeObject(v);
                            counter[pid] += 2;
                            if (counter[pid] >= 100) {
                                oss[pid].reset();
                                counter[pid] = 0;
                            }
                        } catch (ClassNotFoundException | IOException e) {
                            System.err.println("exit");
                            break;
                        }
                    }
                    for (int i = 0; i < n; i++) {
                        oss[i].flush();
                        oss[i].close();
                        bss[i].close();
                        results[i] = bss[i].toByteArray();
                    }
                } catch (IOException e) {
                    System.err.println("error");
                    return;
                }
            }
        });
        t.start();
        t.join();
        System.err.printf("in jvm time usage: %d\n", System.currentTimeMillis() - s);
        return results;
    }

    public static void main(String[] args) throws FileNotFoundException, IOException, InterruptedException {
        File f = new File("/home/lsc/dpx/.test_spill/t0");
        FileInputStream fin = new FileInputStream(f);
        byte[] fall = new byte[(int) f.length()];

        int n_read = fin.read(fall);
        System.err.printf("file length: %d n read: %d\n", f.length(), n_read);
        fin.close();

        byte[][] results = Partitioner.doPartition(fall, 32);
        for (int i = 0; i < 32; i++) {
            File fi = new File("/home/lsc/dpx/.test_spill/t0p" + Integer.toString(i));
            FileOutputStream fout = new FileOutputStream(fi);
            System.err.printf("partition %d: %d\n", i, results[i].length);
            fout.write(results[i]);
            fout.close();
        }
    }
}
