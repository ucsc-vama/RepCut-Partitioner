
#include "CommandlineOptions.h"
#include <iostream>

CommandlineOptions opts;
std::unordered_map<std::string, boost::log::trivial::severity_level> log_levels;
const char* supported_log_levels = "fatal, error, warning, info, debug, trace";

bool parse_commandline_options(int argc, char** argv) {
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("nparts", po::value<int>(), "num of partitions")
            ("graph_file", po::value<std::string>(), "input graph file")
            ("work_directory", po::value<std::string>(), "Working directory")
            ("log_level", po::value<std::string>(), "log level")
            ("target_ib", po::value<float>() ->default_value(0.03f), "target imbalance factor, default 0.03")
            ("threads", po::value<int>(), "parallel threads pass to MtKaHyPar")
            ;

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
    } else {
        opts.graph_filename = "";
    }

    if (vm.count("work_directory")) {
        opts.work_directory = vm["work_directory"].as<std::string>();
    } else {
        opts.work_directory = "";
    }

    if (vm.count("log_level")) {
        opts.log_level = vm["log_level"].as<std::string>();
    } else {
        opts.log_level = "warning";
    }

    if (vm.count("threads")) {
        opts.parallel_threads = vm["threads"].as<int>();
    }

    // Construct log level table

//    trace,
//            debug,
//            info,
//            warning,
//            error,
//            fatal
    log_levels["fatal"] = boost::log::trivial::fatal;
    log_levels["error"] = boost::log::trivial::error;
    log_levels["warning"] = boost::log::trivial::warning;
    log_levels["info"] = boost::log::trivial::info;
    log_levels["debug"] = boost::log::trivial::debug;
    log_levels["trace"] = boost::log::trivial::trace;

    return opts.check();
}

bool CommandlineOptions::check() {
    // Check if input is empty
    if (graph_filename.length() == 0){
        BOOST_LOG_TRIVIAL(fatal) << "Input graph file not given";
        return false;
    }
    // Check graph file status
    auto file_status = fs::status(graph_filename);

    if (!fs::exists(file_status) || !fs::is_regular_file(file_status)) {
        BOOST_LOG_TRIVIAL(fatal) << "Input graph file " << graph_filename << " does not exist or is not a regular file!";
        return false;
    }

    // Check output directory
    auto work_dir_status = fs::status(work_directory);

    if (!fs::exists(work_dir_status) || !fs::is_directory(work_dir_status)) {
        BOOST_LOG_TRIVIAL(fatal) << "Work directory " << work_directory << " does not exist or is not a directory!";
        return false;
    }


    // Check partition size
    if (nparts <= 0 || nparts >= 65535) {
        BOOST_LOG_TRIVIAL(fatal) << "nparts should be within (0, 65535)";
        return false;
    }

    if (!log_levels.contains(log_level)) {
        BOOST_LOG_TRIVIAL(fatal) << "Unsupported log level :" << log_level;
        BOOST_LOG_TRIVIAL(fatal) << "Support options: " << supported_log_levels;
        return false;
    }

//        BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
//        BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
//        BOOST_LOG_TRIVIAL(info) << "An informational severity message";
//        BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
//        BOOST_LOG_TRIVIAL(error) << "An error severity message";
//        BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";
    return true;
}