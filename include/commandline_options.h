#pragma once

#include <string>
#include <filesystem>

#include <boost/log/trivial.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;
namespace fs = std::filesystem;

class CommandlineOptions {
public:
    std::string graph_filename;
    int nparts = -1;
    bool verbose = false;

    bool check() {
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
        BOOST_LOG_TRIVIAL(trace) << "Inout graph file " << graph_filename << " exists.";

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
};

extern CommandlineOptions opts;

bool parse_commandline_options(int argc, char** argv);