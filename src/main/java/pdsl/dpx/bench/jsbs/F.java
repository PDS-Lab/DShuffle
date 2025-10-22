package pdsl.dpx.bench.jsbs;

import java.util.Arrays;

public class F {
    public char[] fake_str;

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        } else if (o == null || getClass() != o.getClass()) {
            return false;
        } else {
            return Arrays.equals(fake_str, ((F) o).fake_str);
        }
    }
}
