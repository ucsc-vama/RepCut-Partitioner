
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
            ("verbose", po::value<bool>(), "verbose output")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 1;
    }

    if (vm.count("nparts")) {
        opts.nparts = vm["nparts"].as<int>();
    }

    if (vm.count("graph_file")) {
        opts.graph_filename = vm["graph_file"].as<std::string>();
    } else {
        opts.graph_filename = "";
    }

    if (vm.count("verbose")) {
        opts.verbose = vm["verbose"].as<bool>();
    }

    return opts.check();
}