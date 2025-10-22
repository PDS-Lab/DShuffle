package pdsl.dpx.bench.jsbs;

public class B {
    public A a;
    public C c;
    public long l;
    public D d;

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            // WARN: for test deep copy
            return false;
        } else if (o == null || getClass() != o.getClass()) {
            return false;
        } else {
            // WARN: here we do not check a, because we will stack into infinite loop.
            B b = (B) o;
            if (c != null ? !c.equals(b.c) : b.c != null) {
                return false;
            }
            if (l != b.l) {
                return false;
            }
            if (d != null ? !d.equals(b.d) : b.d != null) {
                return false;
            }
            return true;
        }
    }
}
