package pdsl.dpx;

import java.io.Closeable;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;

public class SerdeInputStream implements Closeable {
    private Serde sd;
    private InputStream is;
    // private byte[] header = new byte[8];
    private byte[] reuseBuffer = new byte[1 * 1024 * 1024];

    public SerdeInputStream(
            InputStream is) {
        this.is = is;
        this.sd = new Serde();
    }

    private void readInto(byte[] buf) throws IOException {
        readInto(buf, 0, buf.length);
    }

    private void readInto(byte[] buf, int offset, int length) throws IOException {
        int off = offset;
        int remain = length;
        while (true) {
            int rc = is.read(buf, off, remain);
            if (rc == -1) {
                throw new EOFException();
            }
            off += rc;
            remain -= rc;
            if (remain == 0) {
                return;
            }
        }
    }

    public <T> T readObject(Class<T> t) throws IOException {
        readInto(reuseBuffer, 0, 8);
        long length = getLengthOfCurrentObject();
        // byte[] buf = new byte[(int) length];
        if (length > 1 * 1024 * 1024) {
            throw new RuntimeException("Too long");
        }
        // reuseBuffer.put(header);
        // byte[] buf = reuseBuffer.array();
        // System.arraycopy(reuseBuffer, 0, reuseBuffer, 8, (int) length - 8);
        readInto(reuseBuffer, 8, (int) length - 8);
        return sd.Deserialize(reuseBuffer, t);
    }

    private long getLengthOfCurrentObject() {
        return ((long) reuseBuffer[7] << 56)
                | ((long) reuseBuffer[6] & 0xff) << 48
                | ((long) reuseBuffer[5] & 0xff) << 40
                | ((long) reuseBuffer[4] & 0xff) << 32
                | ((long) reuseBuffer[3] & 0xff) << 24
                | ((long) reuseBuffer[2] & 0xff) << 16
                | ((long) reuseBuffer[1] & 0xff) << 8
                | ((long) reuseBuffer[0] & 0xff);
    }

    @Override
    public void close() throws IOException {
        is.close();
        sd.close();
    }
}
