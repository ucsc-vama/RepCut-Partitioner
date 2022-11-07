#include <iostream>

#include "commandline_options.h"
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>

int main(int argc, char** argv) {
    boost::log::add_console_log(std::cout, boost::log::keywords::format = ">> %Message%");
    boost::log::core::get()->set_filter (
            boost::log::trivial::severity >= boost::log::trivial::trace
    );

    if (!parse_commandline_options(argc, argv)) {
        // Some commandline options are illegal
        exit(-1);
    }
    std::cout << "Hello" << std::endl;
    return 0;
}