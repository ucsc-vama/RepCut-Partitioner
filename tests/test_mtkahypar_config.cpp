#include "MtKaHyPar.h"

#include "mt-kahypar/partition/context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
using Context = std::unique_ptr<mt_kahypar_context_t, decltype(&mt_kahypar_free_context)>;

static void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Usage: test-mtkahypar-config WORKDIR");
        fs::create_directories(argv[1]);
        mt_kahypar_initialize(1, false);
        const Context actual(repcut::makeMtKaHyParContext(argv[1]), mt_kahypar_free_context);
        const Context baseline(mt_kahypar_context_from_preset(DEFAULT), mt_kahypar_free_context);
        require(actual && baseline, "Cannot create contexts");

        auto& config = *reinterpret_cast<mt_kahypar::Context*>(actual.get());
        auto& original = *reinterpret_cast<mt_kahypar::Context*>(baseline.get());
        require(config.preprocessing.community_detection.low_memory_contraction,
                "Linked backend did not enable low-memory contraction");
        require(!original.preprocessing.community_detection.low_memory_contraction,
                "Upstream default changed; review RepCut's override");
        require(config.preprocessing.use_community_detection, "Community detection was disabled");
        require(mt_kahypar_get_preset(actual.get()) == DEFAULT, "Wrong preset");
        // The verbose diagnostic includes the other preset options, but not the
        // low-memory flag; compare them to detect an incomplete configuration.
        config.partition.verbose_logging = true;
        original.partition.verbose_logging = true;
        std::ostringstream configured, defaults;
        configured << config;
        defaults << original;
        require(configured.str() == defaults.str(), "Other default preset parameters changed");

        const fs::path file = fs::path(argv[1]) / "mtkahypar.ini";
        require(!fs::exists(file), "Temporary configuration file leaked");
        fs::create_directory(file);
        std::ofstream(file / "sentinel") << "preserve\n";
        bool rejected = false;
        try {
            const Context blocked(repcut::makeMtKaHyParContext(argv[1]), mt_kahypar_free_context);
        }
        catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, "Configuration write failure was ignored");
        require(fs::is_regular_file(file / "sentinel"), "Failed write removed an existing directory");
        fs::remove(file / "sentinel");
        fs::remove(file);
        const Context recovered(repcut::makeMtKaHyParContext(argv[1]), mt_kahypar_free_context);
        require(recovered && !fs::exists(file), "Configuration did not recover after failure");
        std::cout << "Linked low-memory configuration, preset preservation and cleanup passed\n";
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
