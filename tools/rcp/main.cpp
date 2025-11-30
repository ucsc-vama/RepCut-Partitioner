#include <iostream>

#include "commandline_options.h"
#include "dag.h"
#include "cluster_graph.h"
#include "hyper_graph.h"
#include "rep_cut_partitioner.h"
#include "reconstructor.h"

#include <boost/log/utility/setup/console.hpp>

using namespace repcut;


#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/resource.h>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

size_t get_current_rss() {
    // Linux
#if defined(__linux__) || defined(__linux) || defined(linux)
    long rss = 0;
    std::ifstream statm("/proc/self/statm");
    if (statm) {
        size_t total, resident, shared, text, lib, data, dirty;
        statm >> total >> resident >> shared >> text >> lib >> data >> dirty;
        long page_size = sysconf(_SC_PAGESIZE);
        rss = resident * page_size;
    }
    return rss;

    // macOS 
#elif defined(__APPLE__)
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, 
                 (task_info_t)&info, &count) == KERN_SUCCESS) {
        return info.resident_size;
    }
    return 0;

#else
    return 0; // Not supported
#endif
}

void PrintMemoryUsage() {
    size_t rss = get_current_rss();
    std::cout << "Current memory usage: " 
              << rss / (1024 * 1024) << " MB)\n";
}



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
    PrintMemoryUsage();

    // Find all sink Vtxs
    input_dag->findSinkNodes();

    BOOST_LOG_TRIVIAL(info) << "Collapse into cluster graph";
    auto* cluster_graph = new ClusterGraph();
    cluster_graph->parallel_threads = opts.parallel_threads;
    cluster_graph->collapseFromDAG(input_dag);

    PrintMemoryUsage();


    BOOST_LOG_TRIVIAL(info) << "Build hyper graph";
    auto* hyper_graph = new HyperGraph();
    hyper_graph->buildFromClusterGraph(cluster_graph);

    PrintMemoryUsage();

    // Cluster graph is no longer needed once the hypergraph is built.
    // Free it before the MtKaHyPar call and reconstruction to cut peak
    // memory during those phases.
    delete cluster_graph;
    cluster_graph = nullptr;

    PrintMemoryUsage();

    BOOST_LOG_TRIVIAL(info) << "Start Rep Cut partitioner";
    auto* rcp = new RepCutPartitioner();
    rcp -> kahypar_imbalance_factor = opts.target_ib;
    rcp -> hg = hyper_graph;
    rcp -> set_work_directory(opts.work_directory);
    rcp -> write_hmetis();
    // hyper graph is no longer needed. clear
    delete hyper_graph;
    rcp -> hg = nullptr;
    rcp -> parallel_threads = opts.parallel_threads;
    rcp -> partition(opts.nparts);

    PrintMemoryUsage();

    BOOST_LOG_TRIVIAL(info) << "Reconstruct partitions";

    auto reconstructor = new Reconstructor();
    reconstructor -> set_work_directory(opts.work_directory);
    reconstructor -> construct(opts.nparts, input_dag, rcp -> coneIdToPartId);

    PrintMemoryUsage();

    auto stat = reconstructor -> reportPartitionStatus(input_dag);
    stat -> print_stat();
    delete stat;

    PrintMemoryUsage();

    BOOST_LOG_TRIVIAL(info) << "Writing to output file";
    reconstructor -> saveToFile("rcp_output.txt");

    PrintMemoryUsage();

    std::cout << "Done" << std::endl;

    delete reconstructor;
    delete rcp;
//    delete hyper_graph;
    delete input_dag;

    return 0;
}