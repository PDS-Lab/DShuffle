package pdsl.dpx.bench.jsbs;

import java.io.Serializable;

public class Image implements Serializable {
    public enum Size {
        SMALL, LARGE
    }

    public String uri;
    public String title;
    public int width;
    public int height;
    public Size size;

    @Override
    public boolean equals(Object o) {
        if (this == o)
            return true;
        if (o == null || getClass() != o.getClass()) {
            System.err.println("ah??");
            return false;
        }

        Image image = (Image) o;

        if (height != image.height) {
            System.err.println("height??");
            return false;
        }
        if (width != image.width) {
            System.err.println("width??");
            return false;
        }
        if (size != image.size) {
            System.err.println("size??");
            return false;
        }
        if (title != null ? !title.equals(image.title) : image.title != null) {
            System.err.println("title??");
            return false;
        }
        if (uri != null ? !uri.equals(image.uri) : image.uri != null) {
            System.err.println("uri??");
            return false;
        }

        return true;
    }

    @Override
    public String toString() {
        String s = String.format("%s\n", getClass().toString());
        s += String.format("uri: %s\n", uri);
        s += String.format("title: %s\n", title);
        s += String.format("width: %d\n", width);
        s += String.format("height: %d\n", height);
        s += String.format("size: %s\n", size == null ? "null" : size.toString());
        return s;
    }
}
