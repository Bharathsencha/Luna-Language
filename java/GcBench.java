import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Luna GC benchmark suite, Java side.
 * Each benchmark: 1 warmup run + 3 measured runs.
 * Prints one line per measured run:
 *   BENCH <name> <run> <user_ms> <gc_count> <gc_total_ms>
 * GC pause data comes from the JVM's own GC log on stderr
 * (-Xlog:gc:stderr), parsed by the runner.
 */
public class GcBench {

    static volatile Object sink; /* defeats JIT dead-code elimination */

    static List<GarbageCollectorMXBean> beans =
            ManagementFactory.getGarbageCollectorMXBeans();

    static long totalCount() {
        long c = 0;
        for (GarbageCollectorMXBean b : beans) c += b.getCollectionCount();
        return c;
    }

    static long totalTime() {
        long t = 0;
        for (GarbageCollectorMXBean b : beans) t += b.getCollectionTime();
        return t;
    }

    interface Work { void run(); }

    static void bench(String name, Work work) throws Exception {
        work.run(); /* warmup: lets the JIT compile the hot paths */
        System.gc();
        Thread.sleep(50);

        for (int run = 1; run <= 3; run++) {
            long c0 = totalCount();
            long t0 = totalTime();
            long s = System.nanoTime();
            long up0 = ManagementFactory.getRuntimeMXBean().getUptime();
            work.run();
            long ms = (System.nanoTime() - s) / 1_000_000;
            long up1 = ManagementFactory.getRuntimeMXBean().getUptime();
            long gcCount = totalCount() - c0;
            long gcTotal = totalTime() - t0;
            System.out.printf("BENCH %s %d %d %d %d %d %d%n",
                    name, run, ms, gcCount, gcTotal, up0, up1);
            System.out.flush();
        }
    }

    /* ---------- binary_trees ---------- */

    static final class TreeNode {
        TreeNode left, right;
        int value;
        TreeNode(int v) { value = v; }
    }

    static TreeNode buildTree(int depth) {
        if (depth == 0) return null;
        TreeNode n = new TreeNode(1);
        n.left = buildTree(depth - 1);
        n.right = buildTree(depth - 1);
        return n;
    }

    static long treeChecksum(TreeNode n) {
        if (n == null) return 0;
        return n.value + treeChecksum(n.left) + treeChecksum(n.right);
    }

    static void binaryTrees() {
        final int depth = 20; /* ~2M nodes per tree */
        for (int i = 0; i < 5; i++) {
            TreeNode root = buildTree(depth);
            long cs = treeChecksum(root);
            sink = root;
            if (cs < 0) System.out.println(cs);
        }
    }

    /* ---------- map_churn ---------- */

    static void mapChurn() {
        final int ops = 2_000_000;
        final int range = 100_000;
        Map<Integer, Integer> map = new HashMap<>();
        Random r = new Random(42);
        for (int i = 0; i < ops; i++) {
            map.put(r.nextInt(range), r.nextInt());
        }
        for (int i = 0; i < ops; i++) {
            int key = r.nextInt(range);
            switch (r.nextInt(3)) {
                case 0: map.put(r.nextInt(range), r.nextInt()); break;
                case 1: map.remove(key); break;
                default: map.get(key); break;
            }
        }
        sink = map;
    }

    /* ---------- object_graph ---------- */

    static final class GraphNode {
        final List<GraphNode> edges = new java.util.ArrayList<>(4);
        int value;
        GraphNode(int v) { value = v; }
    }

    static void objectGraph() {
        final int count = 500_000;
        Random r = new Random(42);
        GraphNode[] nodes = new GraphNode[count];
        for (int i = 0; i < count; i++) nodes[i] = new GraphNode(i);
        for (int i = 0; i < count; i++) {
            for (int e = 0; e < 4; e++) {
                nodes[i].edges.add(nodes[r.nextInt(count)]);
            }
        }
        for (int i = 0; i < count / 10; i++) {
            GraphNode a = nodes[r.nextInt(count)];
            GraphNode b = nodes[r.nextInt(count)];
            if (!a.edges.isEmpty()) {
                a.edges.set(r.nextInt(a.edges.size()), b);
            }
        }
        long sum = 0;
        for (GraphNode n : nodes) sum += n.edges.size() + n.value;
        sink = nodes;
        if (sum < 0) System.out.println(sum);
    }

    /* ---------- concurrent_map ---------- */

    static void concurrentMap() throws InterruptedException {
        final int threads = 4;
        final int opsPerThread = 500_000;
        final int range = 100_000;
        ConcurrentHashMap<Integer, Integer> map = new ConcurrentHashMap<>();
        Thread[] ts = new Thread[threads];
        for (int t = 0; t < threads; t++) {
            final int seed = 42 + t;
            ts[t] = new Thread(() -> {
                Random r = new Random(seed);
                for (int i = 0; i < opsPerThread; i++) {
                    int key = r.nextInt(range);
                    switch (r.nextInt(3)) {
                        case 0: map.put(r.nextInt(range), r.nextInt()); break;
                        case 1: map.remove(key); break;
                        default: map.get(key); break;
                    }
                }
            });
        }
        for (Thread t : ts) t.start();
        for (Thread t : ts) t.join();
        sink = map;
    }

    /* ---------- classic workloads (same shapes as the Luna/Go suite) ---------- */

    static void allocHeavy() {
        java.util.ArrayList<String> items = new java.util.ArrayList<>(750_000);
        for (int i = 0; i < 750_000; i++) {
            items.add("item-".repeat(2) + Integer.toString(i));
        }
        sink = items;
        if (items.size() != 750_000) System.out.println(items.size());
    }

    static void longLive() {
        java.util.ArrayList<String> table = new java.util.ArrayList<>(200_000);
        for (int i = 0; i < 200_000; i++) {
            table.add("value-".repeat(2) + Integer.toString(i));
        }
        for (int j = 0; j < 400_000; j++) {
            String scratch = "scratch-".repeat(2) + Integer.toString(j);
            if (scratch.isEmpty()) System.out.println("never");
        }
        sink = table;
    }

    static final class CycleNode {
        String name;
        CycleNode next;
        CycleNode(String n) { name = n; }
    }

    static void cycles() {
        java.util.ArrayList<CycleNode> nodes = new java.util.ArrayList<>(50_000);
        for (int i = 0; i < 50_000; i++) {
            nodes.add(new CycleNode("n" + i));
        }
        for (int i = 0; i < nodes.size(); i++) {
            nodes.get(i).next = nodes.get((i + 1) % nodes.size());
        }
        sink = nodes;
        if (nodes.size() != 50_000) System.out.println(nodes.size());
    }

    static void strings() {
        java.util.ArrayList<String> out = new java.util.ArrayList<>(500_000);
        for (int i = 0; i < 500_000; i++) {
            out.add("luna".repeat(16) + "-" + Integer.toString(i));
        }
        sink = out;
        if (out.size() != 500_000) System.out.println(out.size());
    }

    public static void main(String[] args) throws Exception {
        bench("alloc_heavy", GcBench::allocHeavy);
        bench("long_live", GcBench::longLive);
        bench("cycles", GcBench::cycles);
        bench("strings", GcBench::strings);
        bench("binary_trees", GcBench::binaryTrees);
        bench("map_churn", GcBench::mapChurn);
        bench("object_graph", GcBench::objectGraph);
        bench("concurrent_map", () -> {
            try { concurrentMap(); } catch (InterruptedException e) { throw new RuntimeException(e); }
        });
    }
}
