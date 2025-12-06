# RepCut Partitioner

A C++ re-implementation of the RepCut partitioner from *RepCut: Superlinear
Parallel RTL Simulation with Replication-Aided Partitioning* (Wang & Beamer,
ASPLOS 2023).

This release ships both a command-line tool (`rcp`) and a small reentrant C
library (`librepcut`) suitable for embedding into other simulators/tools.

# Requirements & Dependencies

1. Compiler that supports C++20
2. `boost::program_options` — only needed when building the `rcp` CLI tool.
   `librepcut` itself has zero Boost dependency (it links only the bundled
   `tiny-process-library`).
3. The bundled `tiny-process-library` submodule under `external/`.

`MtKaHyPar` binary directory must be in `$PATH` at runtime.

# Build

## Default: CLI tool + library

> mkdir install
> mkdir build && cd build
> cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=../install
> make install

Installs `install/bin/rcp` (the CLI tool) and `install/lib/librepcut.a`.

## Library only (no Boost required)

> mkdir install
> mkdir build && cd build
> cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=../install -DRCP_BUILD_CLI=OFF
> make install

Installs only `install/lib/librepcut.a` (plus headers). `find_package(Boost)`
is not invoked, so Boost need not be installed on the build machine.

# Command Line Options

`--help`               produce help message

`--nparts num`         num of partitions (required)

`--graph_file arg`     input graph file (required)

`--work_directory arg`  Working directory (required). Must be writable; must
                       be **unique per concurrent run** (the tool places a
                       `.lock` file in it as a concurrent-use safeguard).

`--target_ib arg`      target imbalance factor. Default 0.03.

`--threads arg`        parallel threads passed to MtKaHyPar. Only `--threads 1`
                       is deterministic. Only `1` by default.

`--seed arg`           seed passed to MtKaHyPar. `-1` lets MtKaHyPar pick.
                       Set a fixed value (along with `--threads 1`) for
                       reproducible partitioning runs.

`--mtkahypar_bin arg`  path to the MtKaHyPar binary.  If omitted, `rcp` searches
                       `$PATH` for `MtKaHyPar`.  The binary is verified to exist
                       and run before any graph traversal, so a missing binary
                       fails fast with an explicit error.

`--log_level arg`      log level: `silent`, `error`, `warn`, `info`, `debug`.

`-v` / `-vv`           shorthand for `--log_level info` / `--log_level debug`.

By default the tool is silent on success and prints errors to stderr.
Info/debug logs go to stderr; partition statistics (on success) go to stdout.

## Example

To partition `rocket21-1c.graph` into 4 partitions with info-level logging:

> rcp --graph_file ./example/rocket21-1c.graph --work_directory ~/tmp --nparts 4 -v

To reproduce a run exactly:

> rcp --graph_file ./example/rocket21-1c.graph --work_directory ~/tmp --nparts 4 --threads 1 --seed 42

Output file will be written to `rcp_output.txt` under the work directory.

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