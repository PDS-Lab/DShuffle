package pdsl.dpx;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.lang.management.ManagementFactory;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Scanner;
import com.sun.management.*;

@SuppressWarnings("restriction")
public class Options {
    public boolean useDpa;
    public boolean enableUtf16ToUtf8;
    public long maxClassInfoSize;
    public long maxTaskCtxBufferSize;
    public long maxTaskOutBufferSize;
    public long maxDeviceThreads;

    public long maxHeapSize;
    public long minHeapSize;
    public long heapBaseMinAddress;
    public long metaspaceBaseMinAddress;
    public long metaspaceSize;
    public long maxMetaspaceSize;
    public long compressedClassSpaceSize;
    public boolean useCompressedOops;
    public boolean useCompressedClassPointers;

    public String compressedOopsMode;
    public long oopShiftAmount;
    public long narrowKlassBase;
    public long narrowKlassShift;

    public long compressedClassSpaceBase;

    public static final Options defaultOptions = new Options();

    public static HashMap<String, String> jvmOptions = null;

    static {
        jvmOptions = loadJVMOptions();
        defaultOptions.useDpa = true;
        defaultOptions.enableUtf16ToUtf8 = false;
        defaultOptions.maxClassInfoSize = 16 * 1024;
        defaultOptions.maxTaskCtxBufferSize = 128 * 1024;
        defaultOptions.maxTaskOutBufferSize = 16 * 1024;
        defaultOptions.maxDeviceThreads = 1;
        defaultOptions.maxHeapSize = Long.parseLong(jvmOptions.get("MaxHeapSize"));
        defaultOptions.minHeapSize = Long.parseLong(jvmOptions.get("InitialHeapSize"));
        defaultOptions.heapBaseMinAddress = Long.parseLong(jvmOptions.get("HeapBaseMinAddress"));
        defaultOptions.metaspaceSize = Long.parseLong(jvmOptions.get("MetaspaceSize"));
        defaultOptions.maxMetaspaceSize = Long.parseLong(jvmOptions.get("MaxMetaspaceSize"));
        defaultOptions.compressedClassSpaceSize = Long.parseLong(jvmOptions.get("CompressedClassSpaceSize"));
        defaultOptions.useCompressedOops = Boolean.parseBoolean(jvmOptions.get("UseCompressedOops"));
        defaultOptions.useCompressedClassPointers = Boolean.parseBoolean(jvmOptions.get("UseCompressedClassPointers"));
        String cmd = String.format(
                "java -XX:+UnlockDiagnosticVMOptions -XX:+PrintCompressedOopsMode -XX:MaxMetaspaceSize=%d -XX:MetaspaceSize=%d -XX:CompressedClassSpaceSize=%d -Xms%d -Xmx%d -XX:HeapBaseMinAddress=%d -version",
                defaultOptions.maxMetaspaceSize, defaultOptions.metaspaceSize,
                defaultOptions.compressedClassSpaceSize,
                defaultOptions.minHeapSize, defaultOptions.maxHeapSize,
                defaultOptions.heapBaseMinAddress);
        Process p;
        try {
            p = Runtime.getRuntime().exec(cmd);
            p.waitFor();
            BufferedReader r = new BufferedReader(new InputStreamReader(p.getInputStream()));
            for (String s : r.readLine().split(", ")) {
                String[] ss = s.split(": ");
                if (ss[0].contentEquals("heap address")) {
                    long heapBaseMinAddress = Long.parseLong(ss[1].substring(2), 16);
                    if (heapBaseMinAddress != defaultOptions.heapBaseMinAddress) {
                        throw new RuntimeException("Mismatch heap base");
                    }
                } else if (ss[0].contentEquals("size")) {
                    long heapSize = Long.parseLong(ss[1].split(" ")[0]) * 1024 * 1024;
                    if (heapSize != defaultOptions.maxHeapSize
                            || heapSize != defaultOptions.minHeapSize) {
                        System.err.println(heapSize);
                        throw new RuntimeException("Mismatch heapsize");
                    }
                } else if (ss[0].contentEquals("Compressed Oops mode")) {
                    defaultOptions.compressedOopsMode = ss[1];
                } else if (ss[0].contentEquals("Oop shift amount")) {
                    defaultOptions.oopShiftAmount = Long.parseLong(ss[1]);
                }
            }
            for (String s : r.readLine().split(", ")) {
                String[] ss = s.split(": ");
                if (ss[0].contentEquals("Narrow klass base")) {
                    defaultOptions.narrowKlassBase = Long.parseLong(ss[1].substring(2), 16);
                } else if (ss[0].contentEquals("Narrow klass shift")) {
                    defaultOptions.narrowKlassShift = Long.parseLong(ss[1]);
                }
            }
            Scanner s = new Scanner(r.readLine());
            long compressedClassSpaceSize = s.skip("Compressed class space size: ").nextLong();
            if (compressedClassSpaceSize != defaultOptions.compressedClassSpaceSize) {
                s.close();
                throw new RuntimeException("Missmatch compressed class space size");
            }
            defaultOptions.compressedClassSpaceBase = s.skip(" Address: 0x").nextLong(16);
            long compressedClassSpaceBase = s.skip(" Req Addr: 0x").nextLong(16);
            if (compressedClassSpaceBase != defaultOptions.compressedClassSpaceBase) {
                s.close();
                throw new RuntimeException("Missmatched compressed class space base");
            }
            s.close();
        } catch (IOException | InterruptedException e) {
            e.printStackTrace();
        }
    }

    private static HashMap<String, String> loadJVMOptions() {
        final HotSpotDiagnosticMXBean hsdiag = ManagementFactory.getPlatformMXBean(HotSpotDiagnosticMXBean.class);
        List<VMOption> options;
        try {
            final Class<?> flagClass = Class.forName("sun.management.Flag");
            final Method getAllFlagsMethod = flagClass.getDeclaredMethod("getAllFlags");
            final Method getVMOptionMethod = flagClass.getDeclaredMethod("getVMOption");
            getAllFlagsMethod.setAccessible(true);
            getVMOptionMethod.setAccessible(true);
            final Object result = getAllFlagsMethod.invoke(null);
            final List<?> flags = (List<?>) result;
            options = new ArrayList<VMOption>(flags.size());
            for (final Object flag : flags) {
                options.add((VMOption) getVMOptionMethod.invoke(flag));
            }
        } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException
                | InvocationTargetException | ClassCastException e) {
            if (hsdiag != null) { // fallback
                options = hsdiag.getDiagnosticOptions();
            } else {
                options = Collections.emptyList();
            }
        }
        final HashMap<String, String> optionMap = new HashMap<>();
        for (final VMOption option : options) {
            optionMap.put(option.getName(), option.getValue());
        }
        return optionMap;
    }
}