#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

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
    // Seed forwarded to MtKaHyPar via `--seed`.  Default -1 lets MtKaHyPar
    // choose its own seed (typically random).  Set a fixed value for
    // reproducible partitioning runs.
    int seed = -1;

    bool check();
};

extern CommandlineOptions opts;
extern std::unordered_map<std::string, boost::log::trivial::severity_level> log_levels;

bool parse_commandline_options(int argc, char** argv);