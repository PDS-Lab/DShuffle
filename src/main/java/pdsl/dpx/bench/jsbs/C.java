package pdsl.dpx.bench.jsbs;

import java.util.Arrays;

public class C {
    public enum Size {
        SMALL, MEDIUM, LARGE, EXTRALARGE
    }

    public int is;
    public Size s;
    public Size[] ss;

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            // WARN: for test deep copy
            return false;
        } else if (o == null || getClass() != o.getClass()) {
            return false;
        } else {
            C c = (C) o;
            if (s != null ? !s.equals(c.s) : c.s != null) {
                return false;
            }
            if (is != c.is) {
                return false;
            }
            return Arrays.deepEquals(ss, c.ss);
        }
    }
}
