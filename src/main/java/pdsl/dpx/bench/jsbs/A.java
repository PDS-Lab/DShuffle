package pdsl.dpx.bench.jsbs;

public class A {
    public C c;
    public int i;
    public B b;
    public double d;

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            // WARN: for test deep copy
            return false;
        } else if (o == null || getClass() != o.getClass()) {
            return false;
        } else {
            A a = (A) o;
            if (c != null ? !c.equals(a.c) : a.c != null) {
                return false;
            }
            if (i != a.i) {
                return false;
            }
            if (d != a.d) {
                return false;
            }
            if (b != null ? !b.equals(a.b) : a.b != null) {
                return false;
            }
            return true;
        }
    }
}
