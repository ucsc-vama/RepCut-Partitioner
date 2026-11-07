# RepCut Partitioner

A C++ implementation of the RepCut partitioner from *RepCut: Superlinear
Parallel RTL Simulation with Replication-Aided Partitioning* (Wang & Beamer,
ASPLOS 2023).

```bibtex
@inproceedings{wang2023repcut,
  title={Repcut: Superlinear parallel rtl simulation with replication-aided partitioning},
  author={Wang, Haoyuan and Beamer, Scott},
  booktitle={Proceedings of the 28th ACM International Conference on Architectural Support for Programming Languages and Operating Systems, Volume 3},
  pages={572--585},
  year={2023}
}
```

This release ships both a command-line tool (`rcp`) and a small reentrant C
library (`librepcut`) suitable for embedding into other simulators/tools.

# Requirements & Dependencies

1. Compiler that supports C++17
2. `boost::program_options` — only needed when building the `rcp` CLI tool.
   `librepcut` itself has zero Boost dependency (it links only the vendored
   [`tiny-process-library`](https://gitlab.com/eidheim/tiny-process-library)
   sources, which are included in-tree under `external/tiny-process-library/`).

`MtKaHyPar` binary directory must be in `$PATH` or specified.

# Build

## Default: CLI tool + library

> mkdir install \
> mkdir build && cd build \
> cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=../install \
> make install

Installs `install/bin/rcp` (the CLI tool) and `install/lib/librepcut.a`.

## Library only (no Boost required)

> mkdir install \
> mkdir build && cd build \
> cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=../install -DRCP_BUILD_CLI=OFF \
> make install

Installs only `install/lib/librepcut.a` (plus headers).

# Command Line Options

`--help`               produce help message

`--nparts num`         num of partitions (required)

`--graph_file arg`     input graph file (required)

`--work_directory arg`  Working directory (required). Must be writable; must
                       be **unique per concurrent run** (the tool places a
                       `.lock` file in it as a concurrent-use safeguard).

`--target_ib arg`      target imbalance factor. Default 0.03.

`--threads arg`        parallel threads passed to MtKaHyPar. Default `1`.

`--seed arg`           nonnegative seed passed to MtKaHyPar. Default `-1`
                       chooses a random seed. Use `-v` to see the chosen seed.

`--mtkahypar_bin arg`  path to the MtKaHyPar binary.  If omitted, `rcp` searches
                       `$PATH` for `MtKaHyPar`.

`--log_level arg`      log level: `silent`, `error`, `warn`, `info`, `debug`.

`-v` / `-vv`           shorthand for `--log_level info` / `--log_level debug`.

## On `--target_ib` (imbalance target)

When the cone-overlap hypergraph has no edges, the cones are independent.
RepCut assigns whole cones in descending weight order to the least-loaded
partition, breaking ties by cone and partition id. This skips MtKaHyPar's
partitioning call; the early binary availability check still applies. The
existing hypergraph weights, including their positive floor, are used. This
greedy balance is not guaranteed to meet `--target_ib`, especially when one
cone is much heavier than the others. No replication is needed in this case.

RepCut asks MtKaHyPar to do two things:

1. **Balance partition size** — controlled by `--target_ib` (the ε passed to
   MtKaHyPar).  MtKaHyPar tries to constraint deviate from the average by at
   most a factor of ε. (Depending on input graph, this target may be impossible)
2. **Minimize replication** — the km1 objective.  Replication is what breaks
   intra-cycle dependences between partitions, but it also adds work.

The RepCut algorithm itself cannot *a priori* formulate where the replication
will land across partitions.  Even when MtKaHyPar produces a well-balanced
result on the proxy hypergraph, the replication it introduces is distributed
back across the RepCut partitions in a way RepCut does not directly control.
So in situations with high replication, the post-replication RepCut
partitions can still be unbalanced despite a tight `--target_ib`.

This has a less-than-intuitive consequence: **if you observe high
replication, raising `--target_ib` may actually both reduce replication and improve the real balance.**
A larger `target_ib` gives MtKaHyPar more search space to reduce the km1 objective
(replication), and the resulting drop in replication can more than offset
the looser balance target on the proxy hypergraph.

In short:

- The best value of `--target_ib` depends on the input graph.
- `0.03` is simply MtKaHyPar's default and is **likely too small** for many
  RepCut inputs.
- If your partition stats show a high duplication cost, try a larger value
  (e.g. 0.1, 0.3) and compare the resulting balance.

## Example

To partition `rocket21-1c.graph` into 4 partitions with info-level logging:

> rcp --graph_file ./example/rocket21-1c.graph --work_directory ~/tmp --nparts 4 --threads 1 --seed 42

## Output

The partitioned result is written to `${work_directory}/rcp_output.txt`.

Format: one line per partition, containing a comma-separated list of DAG node
ids that the partition must simulate (replicated ancestors included). Node ids
are sorted ascending and correspond to the graph file's line number minus one
(line 1 = node 0, line 2 = node 1, ...).

> `node_id,node_id,...`\
> `node_id,node_id,...`\
> ...

Partition lines are written in partition-id order (0, 1, ..., nparts-1).

# Deterministic

For deterministic partitioning, use a fixed seed (not `-1`) and `--threads 1`.
MtKaHyPar's default preset does not guarantee determinism with multiple threads.

# C Library API

`librepcut` exposes a reentrant extern-C entry point declared in
`include/repcut.h`:

```c
int repcut_run(const struct RepCutContext* ctx,
               struct RepCutStatistics* stats_out);
```

- Returns 0 on success, non-zero on failure.
- On success, writes `rcp_output.txt` under `ctx->work_directory`.
- Reentrant: may be called concurrently from multiple threads **only if**
  each call uses a distinct `work_directory`. A `.lock` file is used as a
  concurrent-use safeguard.
- `ctx->mtkahypar_bin`: if NULL, `librepcut` searches `$PATH` for `MtKaHyPar`;
  if non-NULL, used verbatim. The binary is verified (run with `--version`)
  before any graph traversal, so a missing/unusable binary fails fast with
  an explicit error rather than mid-run.
- By default writes nothing to stderr on success; errors always print.
  Info/debug output is gated by `ctx->log_level`.
- `ctx->seed`: a nonnegative fixed seed, or `-1` for a random seed.
  The chosen value is logged at info level.
- Per-partition statistics are available via the optional `stats_out`
  parameter. (C++ callers can also use `repcut::RepCutPartitioner` directly
  to obtain the full per-partition breakdown.)

# Graph File Format

The input graph file is similar to the Metis input format. The first line
contains number of edges and nodes in the graph, separated by a space:

> `numEdges` `numNodes`

Each following line represents a node in the graph, including its label,
weight, and connecting nodes (out neighbors). Node id starts from 0
(i.e. line 1 is node 0, line 2 is node 1) and fields are separated by
space. A node weight less than 0 indicates an invalid node, and such invalid
nodes will not participate in later partitioning. For correctness, any
invalid node should not have any connecting nodes.

> `Label` `Node Weight` `list of output neighbors (nodes that the current node connects to)`

The value of `Label` does not affect partitioning result. However, `Label` is
mandatory and should not contain spaces. Consecutive spaces between fields
are tolerated by the parser.

An example graph file is under the `example/` directory.

# Tests

The independent-cone regression uses a guarded MtKaHyPar stub to check that
edgeless hypergraphs bypass partitioning, while shared dependencies still
invoke the external tool and propagate its failures. The seed regression
checks seed selection and replay. Python 3 is required:

```sh
python3 tests/test_edgeless.py --rcp build/tools/rcp/rcp --work-directory build/test-edgeless
python3 tests/test_seed.py --rcp build/tools/rcp/rcp --work-directory build/test-seed
python3 tests/test_weights.py --rcp build/tools/rcp/rcp --work-directory build/test-weights
c++ -std=c++17 -Iinclude tests/test_cone_trie.cpp lib/repcut/ConeTrie.cpp -o build/test-cone-trie
build/test-cone-trie
c++ -std=c++17 -Iinclude tests/test_dag.cpp lib/repcut/DAG.cpp -o build/test-dag
build/test-dag build/test-dag-data
```

Generated graphs, memberships and logs remain in the selected work directory.
