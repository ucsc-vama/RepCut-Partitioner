#include <iostream>

#include "commandline_options.h"
#include "dag.h"
#include "cluster_graph.h"
#include "hyper_graph.h"
#include "rep_cut_partitioner.h"
#include "refiner.h"

#include <boost/log/utility/setup/console.hpp>


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
    rcp -> partition();

    BOOST_LOG_TRIVIAL(info) << "Construct partition from RCP result";
    cluster_graph -> constructParts(rcp -> coneIdToPartId);


    auto stat_before_refine = cluster_graph -> reportPartitionStatus();

    stat_before_refine -> print_stat();




    // Refine
    if (opts.refine) {
        BOOST_LOG_TRIVIAL(info) << "Start Refiner";
        auto refine_start = std::chrono::system_clock::now();

        auto* rf = new FMRefiner();
        rf -> cg = cluster_graph;
        rf -> hg = hyper_graph;
        rf -> nparts = opts.nparts;
        rf -> target_ib_factor = opts.target_ib;
        rf -> coneIdToPartId = rcp -> coneIdToPartId;


        rf -> refine();

        auto refine_stop = std::chrono::system_clock::now();
        auto refine_duration = std::chrono::duration_cast<std::chrono::milliseconds>(refine_stop - refine_start);
        uint64_t refine_time_ms = refine_duration.count();
        BOOST_LOG_TRIVIAL(trace) << "Refiner: Done in " << refine_time_ms << "ms";

        BOOST_LOG_TRIVIAL(info) << "Construct partition from Refiner result";
        cluster_graph -> constructParts(rf -> coneIdToPartId);

        auto stat_after_refine = cluster_graph -> reportPartitionStatus();
        stat_after_refine -> print_stat();

        std::cout << "Refine: ib factor from "
                  << stat_before_refine -> ib_factor_weight
                  << " to " << stat_after_refine -> ib_factor_weight << "\n";
        std::cout << "Refine: replication rate from "
                  << stat_before_refine -> replication_rate_weight << "% to "
                  << stat_after_refine -> replication_rate_weight << "%\n";

        float part_weight_max_before = *std::max_element(stat_before_refine -> partition_weights.begin(), stat_before_refine -> partition_weights.end());
        float part_weight_max_after = *std::max_element(stat_after_refine -> partition_weights.begin(), stat_after_refine -> partition_weights.end());

        std::cout << "Refine: max weight from " << part_weight_max_before << " to " << part_weight_max_after << " (" << (part_weight_max_after / part_weight_max_before) << "x)\n";

    }

    cluster_graph -> saveToFile("rcp_output.txt");

    std::cout << "Done" << std::endl;

    return 0;
}