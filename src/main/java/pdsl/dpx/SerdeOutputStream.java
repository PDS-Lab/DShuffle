package pdsl.dpx;

import java.io.Closeable;
import java.io.Flushable;
import java.io.IOException;
import java.io.OutputStream;

public class SerdeOutputStream implements Closeable, Flushable {
    private Serde sd;
    private OutputStream os;

    public SerdeOutputStream(OutputStream os) {
        this.os = os;
        this.sd = new Serde();
    }

    public <T> void writeObject(T obj) throws IOException {
        os.write(sd.Serialize(obj));
    }

    @Override
    public void close() throws IOException {
        os.close();
    }

    @Override
    public void flush() throws IOException {
        os.flush();
        sd.close();
    }
}
