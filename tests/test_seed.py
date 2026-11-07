#!/usr/bin/env python3
"""Check seed selection, forwarding and output lookup with an MtKaHyPar stub."""

import argparse
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class SeedTest(unittest.TestCase):
    def setUp(self):
        self.work = Path(tempfile.mkdtemp(prefix=self._testMethodName + "-", dir=WORK))
        self.graph = self.work / "input.graph"
        # A shared predecessor forces the external partitioning path.
        self.graph.write_text("2 3\nn0 1 1 2\nn1 1\nn2 1\n", encoding="utf8")
        self.mt = self.work / "MtKaHyPar"
        self.mt.write_text(
            "#!/usr/bin/env python3\n"
            "import json, sys\n"
            "from pathlib import Path\n"
            "if sys.argv[1:] == ['--version']:\n"
            "    raise SystemExit(0)\n"
            "args = sys.argv[1:]\n"
            "def option(name):\n"
            "    return args[args.index(name) + 1]\n"
            "graph = Path(option('-h'))\n"
            "(graph.parent / 'call.json').write_text(json.dumps(args))\n"
            "seed = int(option('--seed'))\n"
            "assert 0 <= seed <= 2147483647\n"
            "epsilon = option('-e').rstrip('0')\n"
            "output = str(graph) + '.part' + option('-k')\n"
            "output += '.epsilon' + epsilon + '.seed' + str(seed) + '.KaHyPar'\n"
            "Path(output).write_text('0\\n1\\n')\n",
            encoding="utf8",
        )
        self.mt.chmod(0o755)

    def run_seed(self, seed, label="run", log_level="info", succeeds=True):
        work = self.work / label
        work.mkdir()
        args = [
            str(RCP), "--graph_file", str(self.graph),
            "--work_directory", str(work), "--nparts", "2", "--threads", "1",
            "--mtkahypar_bin", str(self.mt), "--log_level", log_level,
        ]
        if seed is not None:
            args.extend(["--seed", str(seed)])
        result = subprocess.run(args, capture_output=True, text=True, timeout=30)
        (work / "run.log").write_text(result.stdout + result.stderr, encoding="utf8")
        if not succeeds:
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("seed must be -1 or nonnegative", result.stderr)
            self.assertFalse((work / "call.json").exists())
            self.assertFalse((work / "rcp_output.txt").exists())
            return None

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        args = json.loads((work / "call.json").read_text())
        chosen = int(args[args.index("--seed") + 1])
        self.assertGreaterEqual(chosen, 0)
        self.assertLessEqual(chosen, 2147483647)
        self.assertEqual((work / "rcp_output.txt").read_text(), "0,1,\n0,2,\n")
        if log_level == "info":
            reported = re.search(r"Partition seed: (\d+)(.*)", result.stderr)
            self.assertIsNotNone(reported, result.stderr)
            self.assertEqual(int(reported[1]), chosen)
            self.assertEqual("(random)" in reported[2], seed in (None, -1))
        else:
            self.assertEqual(result.stderr, "")
        return chosen

    def test_fixed_seeds(self):
        for seed in (0, 42, 2147483647):
            with self.subTest(seed=seed):
                self.assertEqual(self.run_seed(seed, label=str(seed)), seed)

    def test_random_and_default_seeds(self):
        # Inspect seeds, not membership differences: distinct seeds can produce
        # the same partition. Four identical random draws are vanishingly rare.
        seeds = [self.run_seed(seed, label=str(i))
                 for i, seed in enumerate((-1, None, -1, None))]
        self.assertGreater(len(set(seeds)), 1, seeds)

    def test_replay_selected_seed(self):
        chosen = self.run_seed(-1)
        self.assertEqual(self.run_seed(chosen, label="replay"), chosen)

    def test_invalid_negative_seed(self):
        for seed in (-2, -2147483648):
            with self.subTest(seed=seed):
                self.run_seed(seed, label=str(seed), succeeds=False)

    def test_silent_library_logging(self):
        self.run_seed(-1, log_level="silent")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rcp", required=True, type=Path)
    parser.add_argument("--work-directory", required=True, type=Path)
    args = parser.parse_args()
    RCP = args.rcp.resolve()
    WORK = args.work_directory.resolve()
    WORK.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
