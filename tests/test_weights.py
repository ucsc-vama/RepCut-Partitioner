#!/usr/bin/env python3
"""Check hypergraph weights and dependency closure using an MtKaHyPar stub."""

import argparse
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class WeightTest(unittest.TestCase):
    def setUp(self):
        self.work = Path(tempfile.mkdtemp(prefix=self._testMethodName + "-", dir=WORK))
        self.mt = self.work / "MtKaHyPar"
        self.mt.write_text(
            "#!/usr/bin/env python3\n"
            "import sys\n"
            "from pathlib import Path\n"
            "if sys.argv[1:] == ['--version']:\n"
            "    raise SystemExit(0)\n"
            "def option(name):\n"
            "    return sys.argv[sys.argv.index(name) + 1]\n"
            "graph = Path(option('-h'))\n"
            "cones = int(graph.read_text().splitlines()[0].split()[1])\n"
            "parts = int(option('-k'))\n"
            "output = str(graph) + '.part' + str(parts)\n"
            "output += '.epsilon' + option('-e').rstrip('0')\n"
            "output += '.seed' + option('--seed') + '.KaHyPar'\n"
            "Path(output).write_text(''.join(str(i % parts) + '\\n' for i in range(cones)))\n",
            encoding="utf8",
        )
        self.mt.chmod(0o755)

    def check_graph(self, nodes, expected_hypergraph, expected_parts):
        graph = self.work / "input.graph"
        graph.write_text(
            f"{sum(len(edges) for _, edges in nodes)} {len(nodes)}\n"
            + "".join(
                f"n{nid} {weight} " + " ".join(map(str, edges)) + "\n"
                for nid, (weight, edges) in enumerate(nodes)
            ),
            encoding="utf8",
        )
        result = subprocess.run(
            [str(RCP), "--graph_file", str(graph), "--work_directory", str(self.work),
             "--nparts", "2", "--threads", "1", "--seed", "42",
             "--mtkahypar_bin", str(self.mt)],
            capture_output=True, text=True, timeout=30,
        )
        (self.work / "run.log").write_text(result.stdout + result.stderr, encoding="utf8")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.work / "parts.hmetis").read_text(), expected_hypergraph)
        parts = [
            [int(value) for value in line.split(",") if value]
            for line in (self.work / "rcp_output.txt").read_text().splitlines()
        ]
        self.assertEqual(parts, expected_parts)

    def test_fractional_shared_and_private_weights(self):
        self.check_graph(
            [(2.75, [1, 2]), (0.5, [3]), (6.25, [4]), (1.25, []), (2.75, [])],
            "1 2 11\n3 1 2\n3\n11\n",
            [[0, 1, 3], [0, 2, 4]],
        )

    def test_overlapping_cones(self):
        self.check_graph(
            [(0, [4, 5, 6]), (4.75, [4, 5]), (1.25, [5, 6]), (2, [4]),
             (0, []), (0, []), (0, []), (-1, [])],
            "3 3 11\n1 1 2 3\n5 1 2\n2 2 3\n5\n4\n2\n",
            [[0, 1, 2, 3, 4, 6], [0, 1, 2, 5]],
        )

    def test_round_each_disconnected_cluster_separately(self):
        # Each shared cluster has weight 1, so both contribute zero to each
        # cone. Summing weights before division would incorrectly contribute 1.
        self.check_graph(
            [(0, [2, 3]), (0, [2, 3]), (0, []), (0, [])],
            "2 2 11\n1 1 2\n1 1 2\n1\n1\n",
            [[0, 1, 2], [0, 1, 3]],
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rcp", required=True, type=Path)
    parser.add_argument("--work-directory", required=True, type=Path)
    args = parser.parse_args()
    RCP = args.rcp.resolve()
    WORK = args.work_directory.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
