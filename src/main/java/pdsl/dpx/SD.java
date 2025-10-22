package pdsl.dpx;

import pdsl.dpx.type.*;
import java.util.*;

public class SD {
    // load library
    static {
        // WARN: order of library loading
        System.loadLibrary("dpx_native");
        System.loadLibrary("dpx_sd");
        System.loadLibrary("dpx_trans");
        System.loadLibrary("dpx_common");
    }

    // public normal methods
    public static void Initialize() {
        Initialize(Options.jvmOptions, Options.defaultOptions);
    }

    public static void Initialize(Options o) {
        Initialize(Options.jvmOptions, o);
    }

    public static void Register(TypeTraits<?> traits) {
        Register(traits, null);
    }

    public static void Register(TypeTraits<?> traits, InterfaceTypeMapping mapping) {
        Register(RelatedClassCollector.collect(traits, mapping));
    }

    // public native methods
    public static native void Start();

    public static native void Stop();

    public static native void Destroy();

    public static native void ShowRegisteredClass();

    public static native <T> byte[] Serialize(T obj);

    public static native <T> T Deserialize(byte[] buffer, Class<T> t);

    // private native methods
    private static native void Register(Set<Class<?>> relatedClasses);

    private static native void Initialize(Map<String, String> jvm_options, Options dpx_options);

    // private normal methods
}
