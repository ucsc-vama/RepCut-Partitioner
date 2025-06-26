# RepCut Partitioner

A C++ re-implementation of RepCut partitioner (See essent-parallel-paper repo)

# Requirements & Dependencies

1. Compiler that supports C++20
2. boost::program_options
3. boost::log_setup
4. boost::log

`MtKaHyPar` binary directory must be in `$PATH`

# Build
Create a build directory

> mkdir install
> mkdir build && cd build

Create Makefile:

> cmake  .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=../install

Build & install:

> make install

Binary file is:

> install/bin/rcp

# Command Line Options

`--help`  produce help message

`--threads` number of parallel threads passed to MtKaHyPar

`--target_ib` target imbalance factor. Default value is 0.03

`--nparts num`  num of partitions

`--graph_file arg`  input graph file

`--work_directory arg`  Working directory. RCP must have write permission to create some temporary files

`--log_level arg` log level. Accept:` fatal, error, warning, info, debug, trace`


Example:

To partition graph file `rocket21-1c.graph` into 4 partitions and set log level to `trace`, use following options:

> rcp --graph_file ./example/rocket21-1c.graph --work_directory ~/tmp --nparts 4 --log_level trace

Output file will be written to `rcp_output.txt` under work directory.

# Graph File Format

The input graph file is similar with `Metis` input format. The first line contains number of edges and nodes in the graph (separate by space):
> `numEdges` `numNodes`

Each following line represents a node in the graph, includes its label, weight and connecting nodes (out neighbors). Node id starts from 0 (i.e. line 1 is node 0, line 2 is node 1) and line separated by space. A node weight less than 0 indicates an invalid node, and such invalid nodes will not participate in later partitioning. For correctness, any invalid node should not has any connecting nodes.

> `Label` `Node Weight` `list of output neighbors (nodes that current node connecting to)`

Value of `Label` will not affect partitioning result. However, the `Label` is mandatory and should not contain space.

An example graph files is under `example` directory.
