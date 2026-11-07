#include "ConeTrie.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <vector>

using repcut::ConeTrie;

static void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static void testSharedPaths()
{
    ConeTrie trie;
    require(trie.nodeCount() == 0, "new trie is not empty");
    require(trie.pathConeIds(trie.root()).empty(), "root has a cone");

    const auto a = trie.visit(trie.root(), 0);
    require(trie.visit(trie.root(), 0) == a, "repeated root visit creates a node");
    const auto ab = trie.visit(a, 1);
    const auto b = trie.visit(trie.root(), 1);
    require(trie.visit(a, 1) == ab, "repeated branch visit creates a node");
    const auto ac = trie.visit(a, 2);
    const auto abc = trie.visit(ab, 2);
    require(trie.visit(a, 2) == ac, "interleaved visit loses cached child");
    require(trie.nodeCount() == 5, "unexpected node count");
    require(trie.pathConeIds(a) == std::vector<uint32_t>{0}, "old leaf changed");
    require(trie.pathConeIds(b) == std::vector<uint32_t>{1}, "root branch changed");
    require(trie.pathConeIds(ab) == std::vector<uint32_t>({0, 1}), "old child changed");
    require(trie.pathConeIds(ac) == std::vector<uint32_t>({0, 2}), "branch path changed");
    require(trie.pathConeIds(abc) == std::vector<uint32_t>({0, 1, 2}), "shared prefix changed");
}

static void testArenaGrowth()
{
    ConeTrie trie;
    const auto prefix = trie.visit(trie.root(), 0);
    std::vector<ConeTrie::NodeId> roots, branches;
    // More than two storage chunks, keeping references to every old leaf.
    for (uint32_t cone = 1; cone <= 70000; ++cone) {
        roots.push_back(trie.visit(trie.root(), cone));
        branches.push_back(trie.visit(prefix, cone));
        require(trie.visit(trie.root(), cone) == roots.back(), "root child lost after growth");
        require(trie.visit(prefix, cone) == branches.back(), "prefix child lost after growth");
    }
    require(trie.nodeCount() == 140001, "growth created duplicate nodes");
    for (uint32_t cone = 1; cone <= 70000; ++cone) {
        require(trie.pathConeIds(roots[cone - 1]) == std::vector<uint32_t>{cone}, "old root child invalid");
        require(trie.pathConeIds(branches[cone - 1]) == std::vector<uint32_t>({0, cone}), "old branch child invalid");
    }

    auto leaf = branches.back();
    std::vector<uint32_t> expected{0, 70000};
    for (uint32_t cone = 70001; cone <= 140000; ++cone) {
        leaf = trie.visit(leaf, cone);
        expected.push_back(cone);
    }
    require(trie.pathConeIds(leaf) == expected, "deep path lost across chunks");
}

static void testCanonicalSets()
{
    ConeTrie trie;
    std::mt19937 random(42);
    std::vector<ConeTrie::NodeId> leaves(512, trie.root());
    std::vector<std::vector<uint32_t>> expected(leaves.size());
    for (uint32_t cone = 0; cone < 500; ++cone) {
        for (size_t vertex = 0; vertex < leaves.size(); vertex += 2) {
            if (random() % 3 != 0)
                continue;
            // Paired vertices must share a leaf throughout the traversal.
            for (size_t i = vertex; i < vertex + 2; ++i) {
                leaves[i] = trie.visit(leaves[i], cone);
                expected[i].push_back(cone);
            }
        }
    }
    std::map<std::vector<uint32_t>, ConeTrie::NodeId> canonical;
    for (size_t vertex = 0; vertex < leaves.size(); ++vertex) {
        require(trie.pathConeIds(leaves[vertex]) == expected[vertex], "cone set differs from reference");
        const auto result = canonical.emplace(expected[vertex], leaves[vertex]);
        require(result.first->second == leaves[vertex], "equal cone sets have different leaves");
    }
}

int main()
{
    try {
        testSharedPaths();
        testArenaGrowth();
        testCanonicalSets();
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Cone trie tests passed\n";
}
