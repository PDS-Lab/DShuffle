package pdsl.dpx.bench;

import org.openjdk.jmh.results.format.ResultFormatType;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.RunnerException;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;
import org.openjdk.jmh.runner.options.TimeValue;

public class Benchmark {
    public static void main(String[] args) throws RunnerException {
        System.setProperty("java.vm.name", "Java HotSpot(TM) ");
        if (args.length == 1 && args[0].contentEquals("small")) {
            Options o = (new OptionsBuilder().include(SmallBench.class.getSimpleName())
                    .resultFormat(ResultFormatType.TEXT)
                    .result("./data/small.dpa").forks(1).warmupIterations(5)
                    .warmupTime(TimeValue.seconds(5))
                    .measurementIterations(5).measurementTime(TimeValue.seconds(5))
                    .shouldDoGC(true)).build();
            new Runner(o).run();
        } else if (args.length == 1 && args[0].contentEquals("huge")) {
            for (String arg : new String[] {
                    "64MB", "32MB", "16MB", "8MB", "4MB", "2MB", "1MB", "512KB", "256KB",
                    "128KB", "64KB", "32KB"
            }) {
                Options o = new OptionsBuilder()
                        .include(HugeBench.class.getSimpleName())
                        .resultFormat(ResultFormatType.TEXT)
                        .result("./data/huge.s20." + arg).forks(1).warmupIterations(5)
                        .warmupTime(TimeValue.seconds(5)).measurementIterations(5)
                        .measurementTime(TimeValue.seconds(5)).shouldDoGC(true).param("size", arg)
                        .build();
                new Runner(o).run();
            }
        } else {
            System.err.println("choose bench type: small or huge");
        }
    }
}
