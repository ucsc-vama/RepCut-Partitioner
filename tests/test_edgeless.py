#!/usr/bin/env python3
"""Exercise the CLI with independent cones and a guarded MtKaHyPar stub."""

import argparse
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class EdgelessPartitionTest(unittest.TestCase):
    def setUp(self):
        # Keep all artifacts in the explicitly selected directory for inspection.
        self.work = Path(tempfile.mkdtemp(prefix=self._testMethodName + "-", dir=WORK))
        self.mt = self.work / "MtKaHyPar"
        self.mt.write_text(
            "#!/usr/bin/env python3\n"
            "import json, sys\n"
            "from pathlib import Path\n"
            "with Path(__file__).with_suffix('.calls').open('a') as out:\n"
            "    out.write(json.dumps(sys.argv[1:]) + '\\n')\n"
            "raise SystemExit(0 if sys.argv[1:] == ['--version'] else 91)\n",
            encoding="utf8",
        )
        self.mt.chmod(0o755)

    def run_graph(self, nodes, nparts, label="run", succeeds=True):
        graph = self.work / "input.graph"
        graph.write_text(
            f"{sum(len(edges) for _, edges in nodes)} {len(nodes)}\n"
            + "".join(
                f"n{nid} {weight} " + " ".join(map(str, edges)) + "\n"
                for nid, (weight, edges) in enumerate(nodes)
            ),
            encoding="utf8",
        )
        output = self.work / label
        output.mkdir()
        result = subprocess.run(
            [
                str(RCP),
                "--graph_file",
                str(graph),
                "--work_directory",
                str(output),
                "--nparts",
                str(nparts),
                "--threads",
                "1",
                "--seed",
                "42",
                "--mtkahypar_bin",
                str(self.mt),
                "--log_level",
                "info",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=30,
            check=False,
        )
        (output / "run.log").write_text(result.stdout, encoding="utf8")
        if not succeeds:
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("MtKaHyPar returns non-zero code: 91", result.stdout)
            self.assertFalse((output / "rcp_output.txt").exists())
            return

        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("independent cones", result.stdout)
        self.assertEqual((output / "parts.hmetis").read_text().split()[0], "0")
        parts = [
            [int(value) for value in line.split(",") if value]
            for line in (output / "rcp_output.txt").read_text().splitlines()
        ]
        self.assertEqual(len(parts), nparts)
        members = [node for part in parts for node in part]
        self.assertEqual(
            sorted(members), [n for n, (w, _) in enumerate(nodes) if w >= 0]
        )
        self.assertEqual(
            len(members), len(set(members)), "Independent cones must not replicate"
        )
        for part in parts:
            self.assertEqual(part, sorted(part))
            for nid, (_, edges) in enumerate(nodes):
                for dst in edges:
                    if dst in part:
                        self.assertIn(
                            nid, part, "Partition must retain every predecessor"
                        )
        # The usual early availability check is allowed, but no partition call.
        calls = [
            json.loads(line)
            for line in self.mt.with_suffix(".calls").read_text().splitlines()
        ]
        self.assertTrue(all(call == ["--version"] for call in calls), calls)
        return parts

    def test_weighted_isolated_nodes(self):
        nodes = [(w, []) for w in (3, 7, 4, 6, 5)]
        parts = self.run_graph(nodes, 3)
        # Hypergraph weights include the existing +1 floor: loads are 8, 11, 11.
        self.assertEqual(parts, [[1], [0, 3], [2, 4]])
        self.assertEqual(parts, self.run_graph(nodes, 3, label="repeat"))

    def test_disjoint_cones(self):
        # The original DAG has edges, while its cone-overlap hypergraph has none.
        nodes = [(1, [1]), (4, []), (2, [3]), (1, [4]), (7, []), (0, []), (-1, [])]
        parts = self.run_graph(nodes, 2)
        self.assertEqual(parts, [[2, 3, 4], [0, 1, 5]])

    def test_many_equal_cones(self):
        nodes = []
        for cone in range(227):
            nodes.extend([(1, [2 * cone + 1]), (1, [])])
        parts = self.run_graph(nodes, 3)
        self.assertEqual([len(part) for part in parts], [152, 152, 150])

    def test_zero_weights(self):
        parts = self.run_graph([(0, [])] * 7, 3)
        self.assertEqual([len(part) for part in parts], [3, 2, 2])

    def test_more_partitions_than_cones(self):
        parts = self.run_graph([(1, [])] * 3, 8)
        self.assertEqual(parts, [[0], [1], [2], [], [], [], [], []])

    def test_single_partition(self):
        self.assertEqual(self.run_graph([(1, [])] * 3, 1), [[0, 1, 2]])

    def test_wide_load_sum(self):
        parts = self.run_graph([(1 << 30, [])] * 10, 2)
        self.assertEqual([len(part) for part in parts], [5, 5])

    def test_shared_cones_still_require_mtkahypar(self):
        self.run_graph([(1, [1, 2]), (1, []), (1, [])], 2, succeeds=False)
        calls = [
            json.loads(line)
            for line in self.mt.with_suffix(".calls").read_text().splitlines()
        ]
        self.assertEqual(calls[0], ["--version"])
        self.assertEqual(len(calls), 2)
        self.assertIn("-h", calls[1])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rcp", required=True, type=Path)
    parser.add_argument("--work-directory", required=True, type=Path)
    args = parser.parse_args()
    RCP = args.rcp.resolve()
    WORK = args.work_directory.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
