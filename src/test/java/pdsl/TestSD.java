package pdsl;

import static org.junit.jupiter.api.Assertions.*;

import java.util.ArrayList;
import java.util.List;

import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.BeforeAll;
// import org.junit.jupiter.api.Disabled;
import org.junit.jupiter.api.Test;
// import org.openjdk.jol.info.ClassLayout;
import pdsl.dpx.SD;
import pdsl.dpx.Options;
import pdsl.dpx.bench.jsbs.*;
import pdsl.dpx.type.InterfaceTypeMapping;
import pdsl.dpx.type.TypeTraits;

public class TestSD {
    @BeforeAll
    static void initAll() {
        Options o = Options.defaultOptions;
        o.useDpa = false;
        o.enableUtf16ToUtf8 = false;
        o.maxDeviceThreads = 1;
        SD.Initialize(o);
        SD.Register(new TypeTraits<E>() {});
        SD.Register(new TypeTraits<ArrayList<String>>() {});
        SD.Register(new TypeTraits<String>() {});
        SD.Register(new TypeTraits<A>() {});
        InterfaceTypeMapping m = new InterfaceTypeMapping();
        m.add(new TypeTraits<List<Image>>() {}, new TypeTraits<ArrayList<Image>>() {});
        m.add(new TypeTraits<List<String>>() {}, new TypeTraits<ArrayList<String>>() {});
        SD.Register(new TypeTraits<MediaContent>() {}, m);
        SD.ShowRegisteredClass();
        SD.Start();
    }

    @AfterAll
    static void tearDownAll() {
        SD.Destroy();
    }

    @Test
    void testComplex() {
        A a = new A();
        B b = new B();
        C c = new C();
        D d = new D();

        a.d = 666.666;
        a.i = 114514;
        a.b = b;

        // b.a = a;
        // no recursive
        b.c = c;
        b.d = d;
        b.l = Long.MAX_VALUE - 1;

        c.is = 6;
        c.s = C.Size.LARGE;

        C.Size.LARGE.ordinal();

        c.ss = new C.Size[] {C.Size.EXTRALARGE, C.Size.SMALL, C.Size.EXTRALARGE};

        d.c1 = 'c';
        d.c2 = 'd';
        d.f = Float.MIN_VALUE;

        for (int i = 0; i < 10; i++) {
            byte[] result1 = SD.Serialize(a);
            A get = SD.Deserialize(result1, A.class);
            assertEquals(a, get);
            // assertTrue(get.b.a == get);
            // no recursive
        }
    }

    @Test
    void testArray() {
        long[] vector = new long[] {(long) 6, (long) 6, (long) 6};
        int[][] matrix = new int[][] {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 0, 1, 2},};
        int[][][] cube = new int[][][] {{{1, 2, 3}, {7, 8}}, {{2, 3, 4}, {5, 6, 7}},
                {{3, 4, 5}, {6, 7, 8}, {9, 1, 2}},};

        E e = new E();
        e.vector = vector;
        e.matrix = matrix;
        e.cube = cube;
        F f = new F();
        f.fake_str = new char[] {'1', '1', '4', '5', '1', '4'};
        e.fake_strings = new F[] {f};

        byte[] result2 = SD.Serialize(e);

        E get = SD.Deserialize(result2, E.class);

        assertEquals(e, get);
    }

    @Test
    void testString() {
        String s = "abcdefghijklmnopqrstuvwxyz";
        byte[] r = SD.Serialize(s);
        String t = SD.Deserialize(r, s.getClass());
        assertEquals(s, t);
    }

    @Test
    void testList() {
        List<String> names = new ArrayList<String>();
        names.add("aaa");
        names.add("bbb");
        names.add("ccc");
        names.add("ddd");
        names.add("eee");
        names.add("fff");
        names.add("ggg");
        names.add("hhh");

        byte[] r = SD.Serialize(names);

        ArrayList<?> gotNames = SD.Deserialize(r, ArrayList.class);

        assertIterableEquals(names, gotNames);
    }

    @Test
    void testBasicBench() throws InterruptedException {
        MediaContent mc = MediaContent.BenchCase();
        for (int i = 0; i < 10; i++) {
            byte[] r = SD.Serialize(mc);
            MediaContent got = SD.Deserialize(r, MediaContent.class);
            // System.err.println(mc.images.get(0).size.hashCode());
            // System.err.println(got.images.get(0).size.hashCode());
            if (!mc.equals(got)) {
                Runtime instance = Runtime.getRuntime();
                int mb = 1024 * 1024;
                System.out.println("Total Memory: " + instance.totalMemory() / mb);
                System.out.println("Free Memory: " + instance.freeMemory() / mb);
                System.out.println(
                        "Used Memory: " + (instance.totalMemory() - instance.freeMemory()) / mb);
                System.out.println("Max Memory: " + instance.maxMemory() / mb);
                System.err.println(i);
                System.err.println(mc.toString());
                System.err.println(got.toString());
                break;
            }
        }
    }
}
