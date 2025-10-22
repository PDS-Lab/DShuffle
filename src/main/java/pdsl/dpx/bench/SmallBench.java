package pdsl.dpx.bench;

import java.util.Random;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.TearDown;
import org.openjdk.jmh.annotations.Threads;
import pdsl.dpx.type.TypeTraits;
import pdsl.dpx.SD;
import pdsl.dpx.Options;
import com.esotericsoftware.kryo.Kryo;
import com.esotericsoftware.kryo.io.Input;
import com.esotericsoftware.kryo.io.Output;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import jdk.nashorn.internal.ir.debug.ObjectSizeCalculator;
import java.io.Serializable;

@SuppressWarnings("restriction")
@State(Scope.Benchmark)
@OutputTimeUnit(TimeUnit.MICROSECONDS)
@Threads(1)
public class SmallBench {
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

    static private Pair[][] mc;
    static private byte[] dpx_r;
    // static private byte[] hessian_r;
    static private byte[] kryo_r;
    static private byte[] builtin_r;
    static private Kryo kryo = new Kryo();
    // 4096
    // 2344
    // 1968
    // 1670
    // 2382

    {
        mc = new Pair[1024][];
        for (int j = 0; j < 1024; j++) {
            mc[j] = new Pair[128];
            for (int i = 0; i < 128; i++) {
                mc[j][i] = new Pair();
            }
        }
        // dpx
        Options o = Options.defaultOptions;
        o.useDpa = false;
        o.maxTaskCtxBufferSize = 2 * 1024 * 1024;
        o.maxTaskOutBufferSize = 2 * 1024 * 1024;
        o.enableUtf16ToUtf8 = false;
        SD.Initialize(o);
        // InterfaceTypeMapping mapping = new InterfaceTypeMapping();
        // mapping.add(new TypeTraits<List<Image>>() {
        // }, new TypeTraits<ArrayList<Image>>() {
        // });
        // mapping.add(new TypeTraits<List<String>>() {
        // }, new TypeTraits<ArrayList<String>>() {
        // });
        // SD.Register(new TypeTraits<MediaContent>() {
        // }, mapping);
        SD.Register(new TypeTraits<String>() {});
        SD.Register(new TypeTraits<Pair>() {});
        SD.Register(new TypeTraits<Pair[]>() {});
        SD.Start();
        // dpx_r = SD.Serialize(mc);
        // hessian
        // hessian_r = hessianSerializeBench();
        // kryo
        kryo.setRegistrationRequired(true);
        kryo.register(String.class);
        kryo.register(Pair.class);
        kryo.register(Pair[].class);
        kryo_r = kryoSerializeBench();
        // builtin
        builtin_r = builtinSerializeBench();

        System.err.println(ObjectSizeCalculator.getObjectSize(mc));
        // System.err.println(dpx_r.length);
        // System.err.println(hessian_r.length);
        System.err.println(kryo_r.length);
        // System.err.println(builtin_r.length);
    }

    @TearDown
    static public void tearDown() {
        SD.Destroy();
    }

    // @Benchmark
    // @BenchmarkMode(Mode.AverageTime)
    static public byte[] dpxSerializeBench() {
        return SD.Serialize(mc[new Random().nextInt(1024)]);
    }

    // @Benchmark
    // @BenchmarkMode(Mode.AverageTime)
    static public Pair[] dpxDeserializeBench() {
        return SD.Deserialize(dpx_r, Pair[].class);
    }

    // @Benchmark
    // @BenchmarkMode(Mode.AverageTime)
    // static public MediaContent hessianDeserializeBench()
    // throws IOException, ClassNotFoundException {
    // ByteArrayInputStream inBuf = new ByteArrayInputStream(hessian_r, 0, hessian_r.length);
    // Hessian2Input objInStream = new Hessian2Input(inBuf);
    // MediaContent mc = (MediaContent) objInStream.readObject();
    // objInStream.close();
    // inBuf.close();
    // return mc;
    // }

    // @Benchmark
    // @BenchmarkMode(Mode.AverageTime)
    // static public byte[] hessianSerializeBench() {
    // try {
    // ByteArrayOutputStream outBuf = new ByteArrayOutputStream();
    // Hessian2Output objOutStream = new Hessian2Output(outBuf);
    // objOutStream.writeObject(mc);
    // objOutStream.flush();
    // byte[] result = outBuf.toByteArray();
    // objOutStream.close();
    // outBuf.close();
    // return result;
    // } catch (IOException e) {
    // e.printStackTrace();
    // }
    // return null;
    // }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    static public byte[] kryoSerializeBench() {
        Output out = new Output(128, 6553600);
        kryo.writeClassAndObject(out, mc[new Random().nextInt(1024)]);
        out.close();
        return out.toBytes();
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    static public Pair[] kryoDeserializeBench() {
        Input in = new Input(kryo_r);
        Pair[] mc = (Pair[]) kryo.readClassAndObject(in);
        in.close();
        return mc;
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    static public byte[] builtinSerializeBench() {
        try {
            ByteArrayOutputStream outBuf = new ByteArrayOutputStream();
            ObjectOutputStream objOutStream = new ObjectOutputStream(outBuf);
            objOutStream.writeObject(mc[new Random().nextInt(1024)]);
            byte[] result = outBuf.toByteArray();
            outBuf.close();
            return result;
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    static public Pair[] bulitinDeserializeBench()
            throws IOException, ClassNotFoundException {
        ByteArrayInputStream inBuf = new ByteArrayInputStream(builtin_r, 0, builtin_r.length);
        ObjectInputStream objInStream = new ObjectInputStream(inBuf);
        Pair[] mc = (Pair[]) objInStream.readObject();
        inBuf.close();
        return mc;
    }
}
