#include "CommandlineOptions.h"
#include <iostream>

CommandlineOptions opts;

bool parse_commandline_options(int argc, char** argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")("nparts", po::value<int>(), "num of partitions")(
        "graph_file", po::value<std::string>(), "input graph file")("work_directory", po::value<std::string>(),
                                                                    "Working directory")(
        "log_level", po::value<std::string>(), "log level (silent, error, warn, info, debug)")(
        "target_ib", po::value<float>()->default_value(0.03f), "target imbalance factor, default 0.03")(
        "threads", po::value<int>(), "parallel threads pass to MtKaHyPar (only --threads 1 is deterministic)")(
        "seed", po::value<int>(), "seed pass to MtKaHyPar (-1 = MtKaHyPar default)")(
        "mtkahypar_bin", po::value<std::string>(),
        "path to the MtKaHyPar binary (default: search $PATH for MtKaHyPar)")(
        "verbose,v", po::value<int>()->implicit_value(1)->zero_tokens(), "increase verbosity (-v = info, -vv = debug)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("target_ib")) {
        opts.target_ib = vm["target_ib"].as<float>();
    }

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return false;
    }

    if (vm.count("nparts")) {
        opts.nparts = vm["nparts"].as<int>();
    }

    if (vm.count("graph_file")) {
        opts.graph_filename = vm["graph_file"].as<std::string>();
    }
    else {
        opts.graph_filename = "";
    }

    if (vm.count("work_directory")) {
        opts.work_directory = vm["work_directory"].as<std::string>();
    }
    else {
        opts.work_directory = "";
    }

    if (vm.count("log_level")) {
        opts.log_level = vm["log_level"].as<std::string>();
    }
    else {
        opts.log_level = "";
    }

    if (vm.count("threads")) {
        opts.parallel_threads = vm["threads"].as<int>();
    }

    if (vm.count("seed")) {
        opts.seed = vm["seed"].as<int>();
    }

    if (vm.count("verbose")) {
        opts.verbosity = vm["verbose"].as<int>();
    }

    if (vm.count("mtkahypar_bin")) {
        opts.mtkahypar_bin = vm["mtkahypar_bin"].as<std::string>();
    }

    return opts.check();
}

bool CommandlineOptions::check()
{
    if (graph_filename.empty()) {
        std::fprintf(stderr, "Input graph file not given\n");
        return false;
    }
    auto file_status = fs::status(graph_filename);
    if (!fs::exists(file_status) || !fs::is_regular_file(file_status)) {
        std::fprintf(stderr, "Input graph file %s does not exist or is not a regular file!\n", graph_filename.c_str());
        return false;
    }

    auto work_dir_status = fs::status(work_directory);
    if (!fs::exists(work_dir_status) || !fs::is_directory(work_dir_status)) {
        std::fprintf(stderr, "Work directory %s does not exist or is not a directory!\n", work_directory.c_str());
        return false;
    }

    if (nparts <= 0 || nparts >= 65535) {
        std::fprintf(stderr, "nparts should be within (0, 65535)\n");
        return false;
    }

    return true;
}

RepCutLogLevel resolve_log_level()
{
    // --log_level takes precedence over -v/-vv.
    if (!opts.log_level.empty()) {
        if (opts.log_level == "silent")
            return REPCUT_LOG_SILENT;
        if (opts.log_level == "error")
            return REPCUT_LOG_ERROR;
        if (opts.log_level == "warn")
            return REPCUT_LOG_WARN;
        if (opts.log_level == "info")
            return REPCUT_LOG_INFO;
        if (opts.log_level == "debug")
            return REPCUT_LOG_DEBUG;
        std::fprintf(stderr, "Unsupported log level: %s (silent, error, warn, info, debug)\n", opts.log_level.c_str());
        std::exit(1);
    }
    // Fall back to -v/-vv.
    if (opts.verbosity >= 2)
        return REPCUT_LOG_DEBUG;
    if (opts.verbosity == 1)
        return REPCUT_LOG_INFO;
    return REPCUT_LOG_SILENT;
}