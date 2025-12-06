#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <boost/program_options.hpp>

#include "repcut.h"

namespace fs = std::filesystem;
namespace po = boost::program_options;

class CommandlineOptions
{
public:
    std::string graph_filename;
    fs::path work_directory;
    int nparts = -1;
    std::string log_level;
    float target_ib = 0.03f;
    int parallel_threads = 1;
    int seed = -1;
    // Verbosity (-v=info, -vv=debug).
    int verbosity = 0;
    // Optional MtKaHyPar binary path (empty = search $PATH).
    std::string mtkahypar_bin;

    bool check();
};

extern CommandlineOptions opts;

bool parse_commandline_options(int argc, char** argv);
RepCutLogLevel resolve_log_level();