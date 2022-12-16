//
// Created by Haoyuan Wang on 11/11/22.
//

#include "rep_cut_partitioner.h"

#include <fstream>
#include <iostream>
#include <boost/format.hpp>
#include <boost/process.hpp>


void RepCutPartitioner::_writeKaHyParConfig() {
    auto ofs = std::ofstream(opts.work_directory / this -> kahypar_config_filename);
    ofs << this -> kahypar_config_content;
    ofs.close();
}

void RepCutPartitioner::_callKaHyPar() {
    BOOST_LOG_TRIVIAL(info) << "Call KaHyPar";

    std::vector<std::string> args;

    args.push_back("-h");
    args.push_back(opts.work_directory / this -> hmetis_filename);
    args.push_back("-k");
    args.push_back(std::to_string(this -> desired_parts));
    args.push_back("-e");
    args.push_back(std::to_string(this -> kahypar_imbalance_factor));
    args.push_back("-p");
    args.push_back(opts.work_directory / this -> kahypar_config_filename);
    args.push_back("--seed");
    args.push_back(std::to_string(this -> kahypar_seed));
    args.push_back("-w");
    args.push_back("true");
    args.push_back("--mode");
    args.push_back("direct");
    args.push_back("--objective");
    args.push_back("km1");

    std::error_code ec;
    boost::process::ipstream  is;
    boost::process::child c(boost::process::search_path(this -> kahypar_cmd), args, boost::process::std_out > is);

    std::vector<std::string> data;
    std::string line;

    while (c.running() && std::getline(is, line) && !line.empty()) {
        data.push_back(line);
    }

    c.wait(ec);

    if (ec.value() != 0) {
        std::cout << line << "\n";
        BOOST_LOG_TRIVIAL(fatal) << "KaHyPar returns non-zero code: " << ec.value();
        exit(-1);
    }
}


void RepCutPartitioner::_parseKaHyParResult() {
    auto kahypar_output_fullpath = opts.work_directory / this -> kahypar_output_filename;

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


void RepCutPartitioner::partition() {
    BOOST_LOG_TRIVIAL(trace) << "RepCut Partitioner: Start";
    auto start = std::chrono::system_clock::now();

    assert(this -> hg != nullptr);
    this -> desired_parts = opts.nparts;
    const std::string fmt_str = "%1%.part%2%.epsilon%3%.seed%4%.KaHyPar";
    this -> kahypar_output_filename = (boost::format(fmt_str)
                                                  % this -> hmetis_filename.string()
                                                  % this -> desired_parts
                                                  % this -> kahypar_imbalance_factor
                                                  % this -> kahypar_seed).str();

    // write to hmetis file
    auto hmetis_fullpath = opts.work_directory / this -> hmetis_filename;
    this -> hg -> writeTohMetisFile(hmetis_fullpath.c_str());

    // Write kahypar config file
    this -> _writeKaHyParConfig();

    // Call KaHyPar
    this -> _callKaHyPar();

    this -> _parseKaHyParResult();


    auto stop = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    uint64_t time_ms = duration.count();
    BOOST_LOG_TRIVIAL(trace) << "RepCut Partitioner: Done in " << time_ms << "ms";
}

