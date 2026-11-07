#!/usr/bin/env python3
"""Check optional intermediate dumps without changing partition results."""

import argparse
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class IntermediateTest(unittest.TestCase):
    def setUp(self):
        self.work = Path(tempfile.mkdtemp(prefix=self._testMethodName + "-", dir=WORK))
        self.graph = self.work / "input.graph"
        self.nodes = [
            (0, [4, 5, 6]), (4.75, [4, 5]), (1.25, [5, 6]), (2, [4]),
            (0, []), (0, []), (0, []), (-1, []),
        ]
        self.hypergraph = "3 3 11\n1 1 2 3\n5 1 2\n2 2 3\n5\n4\n2\n"

    def run_case(self, name, save, external=False, seed=42):
        work = self.work / name
        work.mkdir(exist_ok=True)
        self.graph.write_text(
            f"{sum(len(edges) for _, edges in self.nodes)} {len(self.nodes)}\n"
            + "".join(f"n {weight} " + " ".join(map(str, edges)) + "\n"
                      for weight, edges in self.nodes),
            encoding="utf8",
        )
        command = [str(RCP), "--graph_file", str(self.graph), "--work_directory", str(work),
                   "--nparts", "2", "--threads", "1", "--seed", str(seed)]
        if save:
            command.append("--save-intermediate-files")
        if external:
            command.extend(["--mtkahypar_bin", str(self.stub())])
        result = subprocess.run(command, capture_output=True, text=True, timeout=30, check=False)
        (work / "run.log").write_text(result.stdout + result.stderr, encoding="utf8")
        self.assertFalse((work / ".lock").exists(), "Work directory lock leaked")
        return work, result

    def stub(self):
        path = self.work / "MtKaHyPar"
        path.write_text(
            "#!/usr/bin/env python3\n"
            "import sys\n"
            "from pathlib import Path\n"
            "if sys.argv[1:] == ['--version']: raise SystemExit(0)\n"
            "def option(name): return sys.argv[sys.argv.index(name) + 1]\n"
            "graph = Path(option('-h'))\n"
            "cones = int(graph.read_text().splitlines()[0].split()[1])\n"
            "parts = int(option('-k'))\n"
            "output = str(graph) + '.part' + str(parts)\n"
            "output += '.epsilon' + option('-e').rstrip('0')\n"
            "output += '.seed' + option('--seed') + '.KaHyPar'\n"
            "Path(output).write_text(''.join(str(i % parts) + '\\n' for i in range(cones)))\n",
            encoding="utf8",
        )
        path.chmod(0o755)
        return path

    def check_dumps(self, work, seed=42):
        self.assertEqual((work / "parts.hmetis").read_text(), self.hypergraph)
        files = list(work.glob("parts.hmetis.part2.epsilon0.03.seed*.KaHyPar"))
        self.assertEqual(len(files), 1)
        if seed != -1:
            self.assertEqual(files[0].name, f"parts.hmetis.part2.epsilon0.03.seed{seed}.KaHyPar")
        else:
            self.assertGreaterEqual(int(files[0].name.split(".seed")[1].split(".")[0]), 0)
        assignment = [int(line) for line in files[0].read_text().splitlines()]
        sinks = [i for i, (weight, edges) in enumerate(self.nodes) if weight >= 0 and not edges]
        self.assertEqual(len(assignment), len(sinks))
        parents = [[] for _ in self.nodes]
        for node, (_, edges) in enumerate(self.nodes):
            for child in edges:
                parents[child].append(node)
        expected = [set(), set()]
        for sink, part in zip(sinks, assignment):
            self.assertIn(part, (0, 1))
            stack = [sink]
            while stack:
                node = stack.pop()
                if node not in expected[part]:
                    expected[part].add(node)
                    stack.extend(parents[node])
        output = [{int(value) for value in line.split(",") if value}
                  for line in (work / "rcp_output.txt").read_text().splitlines()]
        self.assertEqual(output, expected)

    def test_linked_dump_preserves_results(self):
        if EXTERNAL_ONLY:
            self.skipTest("Linked backend disabled")
        plain, result = self.run_case("plain", False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(list(plain.glob("parts.hmetis*")))
        saved, result = self.run_case("saved", True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.check_dumps(saved)
        self.assertEqual((plain / "rcp_output.txt").read_bytes(), (saved / "rcp_output.txt").read_bytes())

    def test_independent_and_empty(self):
        for name, nodes, hypergraph in [
            ("independent", [(3, []), (7, []), (-1, [])], "0 2 11\n4\n8\n"),
            ("empty", [], "0 0 11\n"),
        ]:
            for external in ([True] if EXTERNAL_ONLY else [False, True]):
                with self.subTest(name=name, external=external):
                    self.nodes, self.hypergraph = nodes, hypergraph
                    work, result = self.run_case(f"{name}-{external}", True, external)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.check_dumps(work)

    def test_external_override(self):
        for save in [False, True]:
            work, result = self.run_case(str(save), save, external=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.check_dumps(work)

    def test_random_seed_filename(self):
        work, result = self.run_case("random", True, external=EXTERNAL_ONLY, seed=-1)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.check_dumps(work, seed=-1)

    def test_hypergraph_write_failure(self):
        work = self.work / "blocked"
        (work / "parts.hmetis").mkdir(parents=True)
        _, result = self.run_case("blocked", True, external=EXTERNAL_ONLY)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot open hypergraph file", result.stderr)
        self.assertFalse((work / "rcp_output.txt").exists())

    def test_assignment_write_failure(self):
        # Independent cones exercise RCP's own assignment writer in either build.
        self.nodes = [(1, []), (1, [])]
        work = self.work / "blocked"
        (work / "parts.hmetis.part2.epsilon0.03.seed42.KaHyPar").mkdir(parents=True)
        _, result = self.run_case("blocked", True, external=EXTERNAL_ONLY)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot write cone assignments", result.stderr)
        self.assertFalse((work / "rcp_output.txt").exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rcp", required=True, type=Path)
    parser.add_argument("--work-directory", required=True, type=Path)
    parser.add_argument("--external-only", action="store_true")
    args = parser.parse_args()
    RCP = args.rcp.resolve()
    WORK = args.work_directory.resolve()
    EXTERNAL_ONLY = args.external_only
    WORK.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
