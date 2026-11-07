#include "DAG.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using repcut::DirectedAcyclicGraph;

static void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

static std::vector<uint32_t> neighbors(repcut::Adjacency::Range range)
{
    std::vector<uint32_t> result;
    for (const auto vertex : range)
        result.push_back(vertex);
    require(range.size() == result.size(), "incorrect range size");
    require(range.empty() == result.empty(), "incorrect empty range");
    return result;
}

static void checkGraph(DirectedAcyclicGraph& dag, const std::string& path, const std::vector<float>& weights,
                       const std::vector<std::vector<uint32_t>>& outgoing, size_t edgeHint)
{
    {
        std::ofstream file(path);
        file << edgeHint << ' ' << weights.size() << '\n';
        for (size_t vertex = 0; vertex < weights.size(); ++vertex) {
            file << 'n' << vertex << ' ' << weights[vertex];
            for (const auto dst : outgoing[vertex])
                file << ' ' << dst;
            file << '\n';
        }
    }
    require(dag.buildFromFile(path.c_str()), "valid graph rejected");
    require(dag.numVertices() == weights.size(), "incorrect vertex count");
    require(dag.inNeigh.size() == weights.size(), "incorrect incoming row count");
    require(dag.outNeigh.size() == weights.size(), "incorrect outgoing row count");
    std::vector<std::vector<uint32_t>> incoming(weights.size());
    std::vector<uint32_t> sinks;
    for (size_t vertex = 0; vertex < weights.size(); ++vertex) {
        const bool valid = weights[vertex] >= 0;
        require(dag.valid[vertex] == valid, "validity changed");
        require(dag.weight[vertex] == (valid ? weights[vertex] : 0), "weight changed");
        const auto expected = valid ? outgoing[vertex] : std::vector<uint32_t>{};
        require(neighbors(dag.outNeigh[vertex]) == expected, "outgoing row order changed");
        for (const auto dst : expected)
            incoming[dst].push_back(static_cast<uint32_t>(vertex));
        if (valid && expected.empty())
            sinks.push_back(static_cast<uint32_t>(vertex));
    }
    for (size_t vertex = 0; vertex < weights.size(); ++vertex)
        require(neighbors(dag.inNeigh[vertex]) == incoming[vertex], "incoming row order changed");
    dag.findSinkNodes();
    require(dag.sinkNodes == sinks, "sink order changed");
    dag.findSinkNodes();
    require(dag.sinkNodes == sinks, "finding sinks twice duplicates them");
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: test-dag WORK_DIRECTORY\n";
        return 2;
    }
    try {
        const std::filesystem::path work(argv[1]);
        std::filesystem::create_directories(work);
        const auto path = (work / "input.graph").string();
        DirectedAcyclicGraph dag;
        checkGraph(dag, path, {1.25f, -1, 0, 2.5f, 1, 0}, {{4, 2, 4}, {99}, {3}, {}, {3}, {}}, 6);
        checkGraph(dag, path, {}, {}, 0);
        checkGraph(dag, path, {0, -1, 1}, {{}, {}, {}}, 0);
        // Under/overestimated edge counts remain capacity hints.
        checkGraph(dag, path, {1, 1, 1}, {{2, 1, 2}, {2}, {}}, 0);
        checkGraph(dag, path, {1, 1, 1}, {{2, 1, 2}, {2}, {}}, 20);

        std::mt19937 random(42);
        for (size_t test = 0; test < 128; ++test) {
            const size_t count = 1 + random() % 257;
            std::vector<float> weights(count, 1);
            std::vector<std::vector<uint32_t>> outgoing(count);
            size_t edges = 0;
            for (size_t src = 0; src < count; ++src) {
                for (size_t dst = src + 1; dst < count; ++dst) {
                    if (random() % 13 != 0)
                        continue;
                    outgoing[src].push_back(static_cast<uint32_t>(dst));
                    ++edges;
                }
            }
            checkGraph(dag, path, weights, outgoing, edges);
        }

        std::vector<float> weights(65538, 1);
        std::vector<std::vector<uint32_t>> outgoing(weights.size());
        for (size_t src = 1; src < weights.size(); ++src)
            outgoing[src].push_back(0);
        checkGraph(dag, path, weights, outgoing, weights.size() - 1);
        checkGraph(dag, path, {1}, {{}}, 0); // Rebuild a smaller DAG in the same object.

        const std::vector<std::string> malformed{
            "",
            "1\n",
            "0 2\nn0 1\n",
            "0 1\nn0 1\nn1 1\n",
            "0 1\nn0\n",
            "1 1\nn0 1 1\n",
            "1 1\nn0 1 -1\n",
            "1 1\nn0 1 4294967296\n",
            "0 4294967296\n",
            "4294967296 0\n",
            "0 1\nn0 bad\n",
        };
        for (const auto& input : malformed) {
            {
                std::ofstream file(path);
                file << input;
            }
            require(!dag.buildFromFile(path.c_str()), "malformed graph accepted");
        }
        const auto missing = (work / "missing.graph").string();
        require(!dag.buildFromFile(missing.c_str()), "missing file accepted");
        checkGraph(dag, path, {1, 1}, {{1}, {}}, 1); // A failed parse must allow a retry.
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "DAG tests passed\n";
}
