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
    fs::path work_directory;
    int nparts = -1;
    bool verbose = false;

    bool check();
};

extern CommandlineOptions opts;

bool parse_commandline_options(int argc, char** argv);