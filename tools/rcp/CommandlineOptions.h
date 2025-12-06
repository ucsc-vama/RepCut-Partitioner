#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <boost/program_options.hpp>

#include "repcut.h"

namespace fs = std::filesystem;
namespace po = boost::program_options;


class CommandlineOptions {
public:
    std::string graph_filename;
    fs::path work_directory;
    int nparts = -1;
    std::string log_level;
    float target_ib = 0.03f;
    int parallel_threads = 1;
    int seed = -1;
    // Verbosity count from -v / -vv.  0 = silent, 1 = info, 2 = debug.
    int verbosity = 0;
    // Optional MtKaHyPar binary path.  If empty, librepcut searches $PATH
    // for "MtKaHyPar".
    std::string mtkahypar_bin;

    bool check();
};

extern CommandlineOptions opts;

// Parse argc/argv into the global `opts`.  Returns false on error or --help.
bool parse_commandline_options(int argc, char** argv);

// Translate the parsed CLI options into the library's RepCutLogLevel.
// If --log_level is given explicitly it takes precedence; otherwise -v / -vv
// determine the level; otherwise silent.
RepCutLogLevel resolve_log_level();