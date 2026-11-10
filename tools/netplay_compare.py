"""Compare OpenShao netplay hash reports from a recording and its replays.

Exit codes: 0 all reports match, 1 differences observed, 2 invalid or incomplete input.
A difference is a measurement result, not a tool failure.
"""
import argparse
import itertools
import json
import struct
import sys
from pathlib import Path

KINDS = ("image", "virtual", "kernel", "physical", "system", "stack")


class ReportError(Exception):
    pass


def recording_frames(path):
    data = Path(path).read_bytes()
    if data[:8] != b"OSNREC\x1a\x00":
        raise ReportError(f"{path}: not an OpenShao recording")
    tag, frames = struct.unpack_from("<II", data, len(data) - 16) if len(data) >= 32 else (0, 0)
    if tag != 0x30444E45:
        raise ReportError(f"{path}: recording has no footer (incomplete)")
    return frames


def load_report(path):
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    if not lines:
        raise ReportError(f"{path}: empty report")
    try:
        records = [json.loads(line) for line in lines]
    except json.JSONDecodeError as error:
        raise ReportError(f"{path}: invalid JSON: {error}") from None
    head, body = records[0], records[1:]
    if head.get("format") != 1 or "hash_every" not in head:
        raise ReportError(f"{path}: unknown report header")
    if not body or not body[-1].get("end"):
        raise ReportError(f"{path}: report has no footer (the run did not finish)")
    footer, snapshots = body[-1], body[:-1]
    every = head["hash_every"]
    previous = 0
    for snap in snapshots:
        frame = snap.get("frame")
        if not isinstance(frame, int) or frame <= previous or (every and frame % every):
            raise ReportError(f"{path}: snapshot frame {frame} breaks the declared cadence of {every}")
        previous = frame
    if footer.get("snapshots") != len(snapshots):
        raise ReportError(f"{path}: footer counts {footer.get('snapshots')} snapshots, report has {len(snapshots)}")
    if every and footer.get("frames", 0) // every != len(snapshots):
        raise ReportError(f"{path}: {len(snapshots)} snapshots do not cover {footer.get('frames')} frames")
    return {"path": str(path), "head": head, "footer": footer,
            "snapshots": {s["frame"]: s for s in snapshots}}


def check_complete(report, frames):
    footer = report["footer"]
    if not footer.get("complete"):
        raise ReportError(f"{report['path']}: the replay stopped before the end of the recording")
    if footer.get("recording_frames") != frames:
        raise ReportError(f"{report['path']}: report is for {footer.get('recording_frames')} frames, recording has {frames}")
    # Replays publish one sample per frame; the record run may stop a frame early at window close.
    if footer.get("mode") == "replay" and footer.get("frames") != frames:
        raise ReportError(f"{report['path']}: replay ended at frame {footer.get('frames')}, recording has {frames}")


def blocks(snapshot):
    return {va: (kind, digest) for va, kind, digest in snapshot["blocks"]}


def compare_pair(a, b, limit):
    frames = sorted(set(a["snapshots"]) & set(b["snapshots"]))
    result = {"frames": frames, "first": None, "kinds": {}, "first_blocks": [], "divergent": 0}
    kind_state = {kind: [] for kind in KINDS}
    for frame in frames:
        sa, sb = a["snapshots"][frame], b["snapshots"][frame]
        differs = sa["total"] != sb["total"]
        result["divergent"] += differs
        ba, bb = blocks(sa), blocks(sb)
        changed = sorted(va for va in set(ba) | set(bb) if ba.get(va) != bb.get(va))
        per_kind = {kind: False for kind in KINDS}
        for va in changed:
            kind = (ba.get(va) or bb.get(va))[0]
            per_kind[kind] = True
        for kind in KINDS:
            kind_state[kind].append(per_kind[kind])
        if differs and result["first"] is None:
            result["first"] = frame
            for va in changed[:limit]:
                kind = (ba.get(va) or bb.get(va))[0]
                presence = "both" if va in ba and va in bb else ("first only" if va in ba else "second only")
                result["first_blocks"].append((va, kind, presence))
            result["first_changed"] = len(changed)
    for kind, states in kind_state.items():
        if not any(states):
            continue
        first = frames[states.index(True)]
        after = states[states.index(True):]
        persistence = "persistent" if all(after) else "transient"
        result["kinds"][kind] = (first, sum(states), persistence)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", nargs="+", type=Path)
    parser.add_argument("--recording", type=Path, help="check that every report covers this whole recording")
    parser.add_argument("--blocks", type=int, default=20, help="blocks listed at the first divergence")
    args = parser.parse_args(argv)
    if len(args.reports) < 2:
        print("Give at least two reports.", file=sys.stderr)
        return 2
    try:
        reports = [load_report(path) for path in args.reports]
        cadences = {r["head"]["hash_every"] for r in reports}
        if len(cadences) != 1 or not cadences.pop():
            raise ReportError("reports use different or no hash cadences")
        if args.recording:
            frames = recording_frames(args.recording)
            for report in reports:
                check_complete(report, frames)
    except (OSError, ReportError) as error:
        print(f"Invalid input: {error}", file=sys.stderr)
        return 2
    identical = True
    for a, b in itertools.combinations(reports, 2):
        r = compare_pair(a, b, args.blocks)
        print(f"== {Path(a['path']).name} vs {Path(b['path']).name}")
        print(f"Snapshots compared: {len(r['frames'])}; divergent: {r['divergent']}")
        if r["first"] is None:
            print("Identical at every compared snapshot.")
            continue
        identical = False
        for kind, (first, count, persistence) in r["kinds"].items():
            print(f"TYPE {kind}: first frame {first}; {count}/{len(r['frames'])} divergent snapshots; {persistence}")
        print(f"First divergent frame: {r['first']}; {r['first_changed']} blocks differ")
        for va, kind, presence in r["first_blocks"]:
            print(f"  0x{va:08X} {kind}" + ("" if presence == "both" else f" ({presence})"))
    return 0 if identical else 1


if __name__ == "__main__":
    sys.exit(main())
