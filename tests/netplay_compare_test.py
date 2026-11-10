"""Regression checks for tools/netplay_compare.py."""
import contextlib
import io
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import netplay_compare  # noqa: E402

BLOCKS = [[0x10000, "image", "aa"], [0x80000000, "kernel", "bb"], [0x80100000, "physical", "cc"]]


def report(path, frames=180, every=60, mode="replay", recording=180, complete=True, change=None, footer=True):
    lines = [{"format": 1, "mode": mode, "hash_every": every, "block": 65536}]
    count = 0
    for frame in range(every, frames + 1, every):
        blocks = [list(b) for b in BLOCKS]
        if change:
            for va, digest in change(frame).items():
                for b in blocks:
                    if b[0] == va:
                        b[2] = digest
        total = "".join(b[2] for b in blocks)
        lines.append({"frame": frame, "total": total, "blocks": blocks})
        count += 1
    if footer:
        lines.append({"end": True, "mode": mode, "frames": frames, "recording_frames": recording,
                      "snapshots": count, "complete": complete})
    path.write_text("".join(json.dumps(line) + "\n" for line in lines), encoding="utf-8")
    return path


def recording(path, frames, footer=True):
    data = b"OSNREC\x1a\x00" + struct.pack("<II", 1, 2) + b"{}" + b"\0" * 4
    if footer:
        data += struct.pack("<IIII", 0x30444E45, frames, 0, 0)
    path.write_bytes(data)
    return path


def run(*argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
        code = netplay_compare.main([str(a) for a in argv])
    return code, out.getvalue()


class CompareTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_identical_reports_pass(self):
        a, b = report(self.dir / "a.jsonl"), report(self.dir / "b.jsonl")
        code, text = run(a, b, "--recording", recording(self.dir / "s.smrec", 180))
        self.assertEqual(code, 0, text)
        self.assertIn("Identical at every compared snapshot", text)

    def test_persistent_and_transient_differences(self):
        a = report(self.dir / "a.jsonl")
        b = report(self.dir / "b.jsonl", change=lambda f: {0x80100000: "zz", **({0x80000000: "yy"} if f == 120 else {})})
        code, text = run(a, b)
        self.assertEqual(code, 1, text)
        self.assertIn("TYPE physical: first frame 60; 3/3 divergent snapshots; persistent", text)
        self.assertIn("TYPE kernel: first frame 120; 1/3 divergent snapshots; transient", text)
        self.assertIn("First divergent frame: 60; 1 blocks differ", text)
        self.assertIn("0x80100000 physical", text)

    def test_missing_block_is_reported(self):
        a = report(self.dir / "a.jsonl")
        b = report(self.dir / "b.jsonl")
        lines = b.read_text().splitlines()
        snap = json.loads(lines[1])
        snap["blocks"] = snap["blocks"][:2]
        snap["total"] = "different"
        lines[1] = json.dumps(snap)
        b.write_text("\n".join(lines) + "\n")
        code, text = run(a, b)
        self.assertEqual(code, 1, text)
        self.assertIn("0x80100000 physical (first only)", text)

    def test_unfinished_report_is_invalid(self):
        code, text = run(report(self.dir / "a.jsonl"), report(self.dir / "b.jsonl", footer=False))
        self.assertEqual(code, 2, text)
        self.assertIn("did not finish", text)

    def test_incomplete_replay_is_invalid(self):
        rec = recording(self.dir / "s.smrec", 180)
        code, text = run(report(self.dir / "a.jsonl"), report(self.dir / "b.jsonl", complete=False), "--recording", rec)
        self.assertEqual(code, 2, text)
        self.assertIn("stopped before the end", text)

    def test_replay_of_another_recording_is_invalid(self):
        rec = recording(self.dir / "s.smrec", 240)
        code, text = run(report(self.dir / "a.jsonl"), report(self.dir / "b.jsonl"), "--recording", rec)
        self.assertEqual(code, 2, text)

    def test_recording_without_footer_is_invalid(self):
        rec = recording(self.dir / "s.smrec", 180, footer=False)
        code, text = run(report(self.dir / "a.jsonl"), report(self.dir / "b.jsonl"), "--recording", rec)
        self.assertEqual(code, 2, text)
        self.assertIn("incomplete", text)

    def test_cadence_and_coverage_checks(self):
        code, _ = run(report(self.dir / "a.jsonl"), report(self.dir / "b.jsonl", every=30))
        self.assertEqual(code, 2)
        bad = report(self.dir / "c.jsonl")
        lines = bad.read_text().splitlines()
        del lines[2]
        bad.write_text("\n".join(lines) + "\n")
        code, text = run(report(self.dir / "a.jsonl"), bad)
        self.assertEqual(code, 2, text)

    def test_record_run_may_end_one_frame_early(self):
        rec = recording(self.dir / "s.smrec", 180)
        record = report(self.dir / "record.jsonl", frames=179, mode="record", recording=180)
        replay = report(self.dir / "replay.jsonl", frames=180, recording=180)
        code, text = run(record, replay, "--recording", rec)
        self.assertEqual(code, 0, text)


if __name__ == "__main__":
    unittest.main()
