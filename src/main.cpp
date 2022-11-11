#include <iostream>

#include "commandline_options.h"
#include "dag.h"
#include "cluster_graph.h"
#include "hyper_graph.h"

#include <boost/log/utility/setup/console.hpp>


int main(int argc, char** argv) {
    boost::log::add_console_log(std::cout, boost::log::keywords::format = ">> %Message%");
    boost::log::core::get()->set_filter (
            boost::log::trivial::severity >= boost::log::trivial::trace
    );

    if (!parse_commandline_options(argc, argv)) {
        // Some commandline options are illegal
        exit(-1);
    }

    BOOST_LOG_TRIVIAL(info) << "Read file";

    auto* input_dag = new DirectedAcyclicGraph();
    input_dag->buildFromFile(opts.graph_filename.c_str());
    bool correct = input_dag->checkCorrectness();
    if (!correct) {
        exit(-1);
    }

    // Find all sink Vtxs
    input_dag->findSinkNodes();

    BOOST_LOG_TRIVIAL(info) << "Collapse into cluster graph";
    auto* cluster_graph = new ClusterGraph();
    cluster_graph->collapseFromDAG(input_dag);


    BOOST_LOG_TRIVIAL(info) << "Build hyper graph";
    auto* hyper_graph = new HyperGraph();
    hyper_graph->buildFromClusterGraph(cluster_graph);

    std::cout << "Done" << std::endl;

    return 0;
}