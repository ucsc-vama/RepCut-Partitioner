//
// Created by Haoyuan Wang on 11/11/22.
//

#include "rep_cut_partitioner.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <sstream>

#include "process.hpp"

using namespace repcut;



void RepCutPartitioner::_callMtKaHyPar() {
    BOOST_LOG_TRIVIAL(info) << "Call MtKaHyPar";
    assert(this->parallel_threads > 0);

    std::vector<std::string> args;

    args.push_back(this->mtkahypar_cmd);

    args.push_back("-t");
    args.push_back(std::to_string(this -> parallel_threads));
    args.push_back("-h");
    args.push_back(work_directory / this -> hmetis_filename);
    args.push_back("-k");
    args.push_back(std::to_string(this -> desired_parts));
    args.push_back("-e");
    args.push_back(std::to_string(this -> kahypar_imbalance_factor));
    args.push_back("--preset-type");
    args.push_back("default");
    args.push_back("--seed");
    args.push_back(std::to_string(this -> kahypar_seed));
    args.push_back("--mode");
    args.push_back("direct");
    args.push_back("-o");
    args.push_back("km1");
    args.push_back("--write-partition-file=true");

    std::ostringstream oss;
    for (const auto &s: args) {
        oss << s << " ";
    }
    BOOST_LOG_TRIVIAL(info) << "MtKaHyPar cmd: " << oss.str() << "\n";

    TinyProcessLib::Process kahypar_process(args, "");
    auto ret_code = kahypar_process.get_exit_status();

    if (ret_code != 0) {
        BOOST_LOG_TRIVIAL(fatal) << "MtKaHyPar returns non-zero code: " << ret_code;
        exit(-1);
    }
}


void RepCutPartitioner::_parseKaHyParResult() {
    auto kahypar_output_fullpath = work_directory / this -> mtkahypar_output_filename;

    auto file_status = fs::status(kahypar_output_fullpath);
    if (!fs::exists(file_status) || !fs::is_regular_file(file_status)) {
        BOOST_LOG_TRIVIAL(fatal) << "KaHyPar result file " << kahypar_output_fullpath << " does not exist or is not a regular file!";
        exit(-1);
    }

    std::ifstream kFile(kahypar_output_fullpath);
    std::string line;
    uint32_t lineno = 0;

    this -> coneIdToPartId.clear();
    this -> partIdToConeId.clear();
    this -> partIdToConeId.assign(this -> desired_parts, std::vector<uint32_t>());

    while (std::getline(kFile, line)) {
        boost::trim(line);
        int32_t pid = std::stoi(line);
        // cone ${lineno} is assigned to partition ${pid}
        assert(pid < this -> desired_parts);
        this -> coneIdToPartId.push_back(pid);
        this -> partIdToConeId[pid].push_back(lineno);

        lineno++;
    }
}

void RepCutPartitioner::write_hmetis() {
    // write to hmetis file
    auto hmetis_fullpath = work_directory / this -> hmetis_filename;
    this -> hg -> writeTohMetisFile(hmetis_fullpath.c_str());
}


void RepCutPartitioner::partition(const int nparts) {
    BOOST_LOG_TRIVIAL(trace) << "RepCut Partitioner: Start";
    auto start = std::chrono::system_clock::now();

    this -> desired_parts = nparts;
    const std::string fmt_str = "%1%.part%2%.epsilon%3%.seed%4%.KaHyPar";
    this -> mtkahypar_output_filename = (boost::format(fmt_str)
                                                  % this -> hmetis_filename.string()
                                                  % this -> desired_parts
                                                  % this -> kahypar_imbalance_factor
                                                  % this -> kahypar_seed).str();


    // Call mtKaHyPar
    this -> _callMtKaHyPar();

    this -> _parseKaHyParResult();


    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "RepCut Partitioner: Done in " << time_ms << "ms";
}

