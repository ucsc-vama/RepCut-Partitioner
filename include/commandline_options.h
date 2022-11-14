#pragma once

#include "rcp_common.h"

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;


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