package pdsl.dpx.bench.jsbs;

import java.util.Arrays;

public class E {
    public long[] vector;
    public int[][] matrix;
    public int[][][] cube;
    public F[] fake_strings;

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        } else if (o == null || o.getClass() != getClass()) {
            return false;
        } else {
            E e = (E) o;
            return Arrays.equals(vector, e.vector) && Arrays.deepEquals(matrix, e.matrix)
                    && Arrays.deepEquals(cube, e.cube)
                    && Arrays.deepEquals(fake_strings, e.fake_strings);
        }
    }
}
