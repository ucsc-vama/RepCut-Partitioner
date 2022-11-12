
#include "commandline_options.h"
#include <iostream>

CommandlineOptions opts;

bool parse_commandline_options(int argc, char** argv) {
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("nparts", po::value<int>(), "set compression level")
            ("graph_file", po::value<std::string>(), "input graph file")
            ("work_directory", po::value<std::string>(), "Working directory")
            ("verbose", po::value<bool>(), "verbose output")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

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

    if (vm.count("verbose")) {
        opts.verbose = vm["verbose"].as<bool>();
    }

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
    if (nparts <= 0 || nparts >= 1024) {
        BOOST_LOG_TRIVIAL(fatal) << "nparts should be within (0, 1024)";
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