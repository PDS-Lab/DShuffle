package pdsl.dpx.bench.jsbs;

import java.util.Arrays;

public class D {
    public char c1;
    public float f;
    public char c2;
    public double[] ds;


    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        } else if (o == null || getClass() != o.getClass()) {
            return false;
        } else {
            D d = (D) o;
            return c1 == d.c1 && f == d.f && c2 == d.c2 && Arrays.equals(ds, d.ds);
        }
    }
}
