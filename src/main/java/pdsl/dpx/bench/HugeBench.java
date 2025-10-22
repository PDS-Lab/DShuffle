package pdsl.dpx.bench;

import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.TimeUnit;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
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
import com.caucho.hessian.io.Hessian2Input;
import com.caucho.hessian.io.Hessian2Output;
import jdk.nashorn.internal.ir.debug.ObjectSizeCalculator;

@SuppressWarnings("restriction")
@State(Scope.Benchmark)
@OutputTimeUnit(TimeUnit.MICROSECONDS)
@Threads(1)
public class HugeBench {
    private double[][] m;
    private byte[] dpx_r;
    private byte[] hessian_r;
    private byte[] kryo_r;
    private byte[] builtin_r;
    private Kryo kryo = new Kryo();
    static private Map<String, int[]> size_m = new HashMap<>();

    @Param({ "32KB" })
    public String size;

    static {
        size_m.put("32KB", new int[] { 64, 64 });
        size_m.put("64KB", new int[] { 128, 64 });
        size_m.put("128KB", new int[] { 128, 128 });
        size_m.put("256KB", new int[] { 256, 128 });
        size_m.put("512KB", new int[] { 256, 256 });
        size_m.put("1MB", new int[] { 512, 256 });
        size_m.put("2MB", new int[] { 512, 512 });
        size_m.put("4MB", new int[] { 1024, 512 });
        size_m.put("8MB", new int[] { 1024, 1024 });
        size_m.put("16MB", new int[] { 1024, 2048 });
        size_m.put("32MB", new int[] { 2048, 2048 });
        size_m.put("64MB", new int[] { 2048, 4096 });
        size_m.put("128MB", new int[] { 2048, 8192 });
        size_m.put("256MB", new int[] { 2048, 16384 });
    }

    static public long get_actual_size(String size) {
        if (size.endsWith("KB")) {
            return 1024 * Long.parseLong(size.substring(0, size.length() - 2));
        } else if (size.endsWith("MB")) {
            return 1024 * 1024 * Long.parseLong(size.substring(0, size.length() - 2));
        } else {
            throw new RuntimeException("Unknown suffix");
        }
    }

    @Setup
    public void setUp() {
        long sz = get_actual_size(size);
        System.err.println(size);
        System.err.println(sz);
        int[] rc = size_m.get(size);
        int r = rc[0];
        int c = rc[1];
        m = new double[r][c];
        double max = 1e18 + 7;
        for (int i = 0; i < r; i++) {
            for (int j = 0; j < c; j++) {
                m[i][j] = new Random().nextDouble() * max;
            }
        }

        // dpx
        Options o = Options.defaultOptions;
        o.useDpa = false;
        o.enableUtf16ToUtf8 = true;
        o.maxTaskOutBufferSize = sz + (sz >> 2);
        o.maxDeviceThreads = 16;
        SD.Initialize(o);
        SD.Register(new TypeTraits<double[][]>() {
        });
        SD.Start();
        dpx_r = SD.Serialize(m);
        // hessian
        hessian_r = hessianSerializeBench();
        // kryo
        kryo.setRegistrationRequired(true);
        kryo.register(double[].class);
        kryo.register(double[][].class);
        kryo_r = kryoSerializeBench();
        // builtin
        builtin_r = builtinSerializeBench();

        System.err.println(ObjectSizeCalculator.getObjectSize(m));
        System.err.println(dpx_r.length);
        System.err.println(hessian_r.length);
        System.err.println(kryo_r.length);
        System.err.println(builtin_r.length);
    }

    @TearDown
    public void tearDown() {
        SD.Destroy();
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    public byte[] dpxSerializeBench() {
        return SD.Serialize(m);
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    public double[][] dpxDeserializeBench() {
        return SD.Deserialize(dpx_r, double[][].class);
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    public double[][] hessianDeserializeBench() throws IOException, ClassNotFoundException {
        ByteArrayInputStream inBuf = new ByteArrayInputStream(hessian_r, 0, hessian_r.length);
        Hessian2Input objInStream = new Hessian2Input(inBuf);
        double[][] mc = (double[][]) objInStream.readObject();
        objInStream.close();
        inBuf.close();
        return mc;
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    public byte[] hessianSerializeBench() {
        try {
            ByteArrayOutputStream outBuf = new ByteArrayOutputStream();
            Hessian2Output objOutStream = new Hessian2Output(outBuf);
            objOutStream.writeObject(m);
            objOutStream.flush();
            byte[] result = outBuf.toByteArray();
            objOutStream.close();
            outBuf.close();
            return result;
        } catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    public byte[] kryoSerializeBench() {
        Output out = new Output(m.length * m[0].length * 10);
        kryo.writeClassAndObject(out, m);
        out.close();
        return out.toBytes();
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    public double[][] kryoDeserializeBench() {
        Input in = new Input(kryo_r);
        double[][] mc = (double[][]) kryo.readClassAndObject(in);
        in.close();
        return mc;
    }

    @Benchmark
    @BenchmarkMode(Mode.AverageTime)
    public byte[] builtinSerializeBench() {
        try {
            ByteArrayOutputStream outBuf = new ByteArrayOutputStream();
            ObjectOutputStream objOutStream = new ObjectOutputStream(outBuf);
            objOutStream.writeObject(m);
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
    public double[][] bulitinDeserializeBench() throws IOException, ClassNotFoundException {
        ByteArrayInputStream inBuf = new ByteArrayInputStream(builtin_r, 0, builtin_r.length);
        ObjectInputStream objInStream = new ObjectInputStream(inBuf);
        double[][] mc = (double[][]) objInStream.readObject();
        inBuf.close();
        return mc;
    }
}
