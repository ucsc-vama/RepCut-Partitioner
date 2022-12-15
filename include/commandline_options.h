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
    std::string log_level;
    bool refine = true;

    bool check();
};

extern CommandlineOptions opts;
extern std::unordered_map<std::string, boost::log::trivial::severity_level> log_levels;

bool parse_commandline_options(int argc, char** argv);