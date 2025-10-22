package pdsl.dpx.bench;

import pdsl.dpx.type.TypeTraits;
import pdsl.dpx.SD;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.Random;
import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.io.Output;
import pdsl.dpx.Options;

public class SerdeBench {

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

    static public class Pair implements Serializable {
        public String key = generateRandomASCII(4000);
        public String value = generateRandomASCII(4000);
    }

    static public void main(String args[]) {
        Pair[] ps = new Pair[128];
        for (int i = 0; i < 128; i++) {
            ps[i] = new Pair();
        }
        Kryo kryo = new Kryo();
        kryo.setRegistrationRequired(true);
        kryo.register(String.class);
        kryo.register(Pair.class);
        kryo.register(Pair[].class);
        // Options o = Options.defaultOptions;
        // o.useDpa = true;
        // o.maxDeviceThreads = Integer.parseInt(args[0]);
        // o.maxTaskCtxBufferSize = 2 * 1024 * 1024;
        // o.maxTaskOutBufferSize = 2 * 1024 * 1024;
        // SD.Initialize(o);
        // SD.Register(new TypeTraits<String>() {});
        // SD.Register(new TypeTraits<Pair>() {});
        // SD.Register(new TypeTraits<Pair[]>() {});
        // SD.Start();
        byte[] r = null;
        long s = System.nanoTime();


        Output out = new Output(2048, 65536 * 100);
        kryo.writeClassAndObject(out, ps);
        out.close();
        r = out.toBytes();

        // try {
        // ByteArrayOutputStream outBuf = new ByteArrayOutputStream();
        // ObjectOutputStream objOutStream = new ObjectOutputStream(outBuf);
        // objOutStream.writeObject(ps);
        // r = outBuf.toByteArray();
        // outBuf.close();
        // } catch (IOException e) {
        // e.printStackTrace();
        // }



        // byte[] r = SD.Serialize(ps);
        long e = System.nanoTime();

        System.err.printf("r: %d use %d\n", r.length, e - s);

        // SD.Stop();
        // SD.Destroy();
    }
}
