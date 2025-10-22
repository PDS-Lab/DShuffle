package pdsl.dpx;

public class TransEnv {
    static {
        System.loadLibrary("dpx_native");
        System.loadLibrary("dpx_sd");
        System.loadLibrary("dpx_trans");
        System.loadLibrary("dpx_common");
    }

    public static native void Initialize(String dev_pci_addr, String mount_point,
            String output_device);

    public static native void TriggerSpillStart();

    public static native void Append(int partitionId, byte[] key, byte[] value, boolean last);

    public static native void WaitForSpillDone();

    public static native void Destroy();
}
