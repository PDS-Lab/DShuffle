package pdsl.dpx;

public class NaiveTransEnv {
    static {
        System.loadLibrary("dpx_native");
        System.loadLibrary("dpx_sd");
        System.loadLibrary("dpx_trans");
        System.loadLibrary("dpx_common");
    }

    public static native void Initialize(String dev_pci_addr, String spill_dir);

    public static native void TriggerSpillStart(boolean need_header);

    public static native void Spill(byte[] data);

    public static native void WaitForSpillDone();

    public static native void Destroy();
}
