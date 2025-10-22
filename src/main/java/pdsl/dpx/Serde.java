package pdsl.dpx;

import java.util.Map;
import java.util.Set;

import pdsl.dpx.type.InterfaceTypeMapping;
import pdsl.dpx.type.RelatedClassCollector;
import pdsl.dpx.type.TypeTraits;

public class Serde {
    // load library
    static {
        // WARN: order of library loading
        System.loadLibrary("dpx_native");
        System.loadLibrary("dpx_sd");
        System.loadLibrary("dpx_trans");
        System.loadLibrary("dpx_common");
    }

    private long handle;

    public Serde() {
        handle = create();
        if (handle == 0) {
            throw new RuntimeException("Fail to create Serde");
        }
    }

    public <T> byte[] Serialize(T obj) {
        return Serialize(handle, obj);
    }

    public <T> T Deserialize(byte[] buffer, Class<T> t) {
        return Deserialize(handle, buffer, t);
    }

    public void close() {
        close(handle);
    }

    // static

    // public normal methods
    public static void Initialize() {
        Initialize(Options.jvmOptions, Options.defaultOptions);
        PreRegister();
    }

    public static void Initialize(Options o) {
        Initialize(Options.jvmOptions, o);
        PreRegister();
    }

    public static void Register(TypeTraits<?> traits) {
        Register(traits, null);
    }

    public static void Register(TypeTraits<?> traits, InterfaceTypeMapping mapping) {
        Register(RelatedClassCollector.collect(traits, mapping));
    }

    // public native methods
    public static native void Destroy();

    public static native void ShowRegisteredClass();

    // private native methods
    private static void PreRegister() {
        Register(new TypeTraits<byte[]>() {});
        Register(new TypeTraits<char[]>() {});
        Register(new TypeTraits<String>() {});
        Register(new TypeTraits<Integer>() {});
    }

    private static native <T> byte[] Serialize(long handle, T obj);

    private static native <T> T Deserialize(long handle, byte[] buffer, Class<T> t);

    private static native void Register(Set<Class<?>> relatedClasses);

    private static native void Initialize(Map<String, String> jvm_options, Options dpx_options);

    private static native long create();

    private static native void close(long handle);
}
