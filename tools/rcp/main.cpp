#include <iostream>

#include "commandline_options.h"
#include "dag.h"
#include "cluster_graph.h"
#include "hyper_graph.h"
#include "rep_cut_partitioner.h"
#include "reconstructor.h"

#include <boost/log/utility/setup/console.hpp>

using namespace repcut;


int main(int argc, char** argv) {
    boost::log::add_console_log(std::cout, boost::log::keywords::format = ">> %Message%");
    boost::log::core::get()->set_filter (
            boost::log::trivial::severity >= boost::log::trivial::warning
    );

    if (!parse_commandline_options(argc, argv)) {
        // Some commandline options are illegal
        exit(-1);
    }
    // set log level
    auto boost_log_level = log_levels[opts.log_level];
    boost::log::core::get()->set_filter (
            boost::log::trivial::severity >= boost_log_level
    );
    BOOST_LOG_TRIVIAL(info) << "Set log level to " << opts.log_level;


    BOOST_LOG_TRIVIAL(info) << "Read file";

    auto* input_dag = new DirectedAcyclicGraph();
    input_dag->buildFromFile(opts.graph_filename.c_str());
    bool correct = input_dag->checkCorrectness();
    if (!correct) exit(-1);

    // Find all sink Vtxs
    input_dag->findSinkNodes();

    BOOST_LOG_TRIVIAL(info) << "Collapse into cluster graph";
    auto* cluster_graph = new ClusterGraph();
    cluster_graph->collapseFromDAG(input_dag);


    BOOST_LOG_TRIVIAL(info) << "Build hyper graph";
    auto* hyper_graph = new HyperGraph();
    hyper_graph->buildFromClusterGraph(cluster_graph);

    BOOST_LOG_TRIVIAL(info) << "Start Rep Cut partitioner";
    auto* rcp = new RepCutPartitioner();
    rcp -> hg = hyper_graph;
    rcp -> set_work_directory(opts.work_directory);
    rcp -> partition(opts.nparts);

    BOOST_LOG_TRIVIAL(info) << "Construct partition from RCP result";
    cluster_graph -> constructParts(opts.nparts, rcp -> coneIdToPartId);

    BOOST_LOG_TRIVIAL(info) << "Reconstruct partitions";

    auto reconstructor = new Reconstructor();
    reconstructor -> set_work_directory(opts.work_directory);
    reconstructor -> construct(opts.nparts, cluster_graph);

    BOOST_LOG_TRIVIAL(info) << "Writing to output file";
    reconstructor -> saveToFile("rcp_output.txt");

    std::cout << "Done" << std::endl;

    delete reconstructor;
    delete rcp;
    delete hyper_graph;
    delete cluster_graph;
    delete input_dag;

    return 0;
}