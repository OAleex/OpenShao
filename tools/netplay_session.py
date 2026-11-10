"""Record a session and replay it in isolated copies to measure determinism.

  seed    copy the data folder and the save folder into a frozen starting point
  record  play visibly from a fresh copy of the seed, writing session.smrec
  replay  run hidden replays, each from its own fresh copy of the seed
  compare compare the record report and every replay report

Each run gets its own data and save folders, so the player's real data is only read.
The executable is copied into the session folder at record time; replays use that copy.
"""
import argparse
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import netplay_compare  # noqa: E402

DATA_SKIP = {"logs", "crashes", "reports", ".lock"}
CACHE_SKIP = {"update", "support"}
MISMATCH = 60


def copy_data(source, target):
    target.mkdir(parents=True)
    for item in source.iterdir():
        if item.name in DATA_SKIP or item.suffix == ".smrec":
            continue
        if item.name == "cache" and item.is_dir():
            shutil.copytree(item, target / "cache", ignore=lambda d, names: [n for n in names if Path(d) == item and n in CACHE_SKIP])
        elif item.is_dir():
            shutil.copytree(item, target / item.name)
        else:
            shutil.copy2(item, target / item.name)


def find_save_folder(exe, data):
    # Ask the game: --paths prints the configured folders. Logs hide paths for privacy.
    # Run it as a normal start would, then check that it describes the same data folder.
    result = subprocess.run([str(exe), "--paths"], capture_output=True, text=True, timeout=60)
    try:
        text = result.stdout
        folders = json.loads(text[text.index("{"):text.rindex("}") + 1])
        saves = Path(folders["saves"])
        if Path(folders["data"]).resolve() != data:
            return None
    except (IndexError, KeyError, ValueError, OSError):
        return None
    folder = saves / "hdd"
    return folder if folder.is_dir() else None


def fresh_run(seed, folder):
    if folder.exists():
        shutil.rmtree(folder)
    shutil.copytree(seed / "data", folder / "data")
    shutil.copytree(seed / "saves", folder / "saves")
    return folder / "data", folder / "saves"


def cmd_seed(args):
    data = args.data.resolve()
    saves = args.saves or find_save_folder(args.exe.resolve(), data)
    if not saves:
        print("Could not find the save folder; pass --saves.", file=sys.stderr)
        return 2
    print(f"Save folder: {saves}")
    out = args.out.resolve()
    if out.exists():
        print(f"{out} already exists; choose a new seed folder.", file=sys.stderr)
        return 2
    copy_data(data, out / "data")
    shutil.copytree(saves, out / "saves")
    (out / "seed.json").write_text(json.dumps({"data": str(data), "saves": str(saves.resolve())}, indent=2), encoding="utf-8")
    print(f"Seed ready: {out}")
    return 0


def cmd_record(args):
    out = args.out.resolve()
    if (out / "session.smrec").exists():
        print(f"{out} already has a recording.", file=sys.stderr)
        return 2
    out.mkdir(parents=True, exist_ok=True)
    exe = out / args.exe.name
    shutil.copy2(args.exe, exe)
    data, saves = fresh_run(args.seed.resolve(), out / "record")
    command = [str(exe), "--no-launcher", "--data-dir", str(data), "--save-dir", str(saves),
               "--record", str(out / "session.smrec"), "--hash-every", str(args.hash_every),
               "--hash-out", str(out / "record.jsonl")]
    print("Recording. Play, then close the game window to finish.")
    code = subprocess.call(command)
    print(f"Game exited with {code}.")
    try:
        frames = netplay_compare.recording_frames(out / "session.smrec")
    except (OSError, netplay_compare.ReportError) as error:
        print(f"Recording is not usable: {error}", file=sys.stderr)
        return 2
    print(f"Recording complete: {frames} frames ({frames / 60:.0f} s at 60 frames per second).")
    return 0


def cmd_replay(args):
    out = args.out.resolve()
    exe = out / args.exe_name
    recording = out / "session.smrec"
    frames = netplay_compare.recording_frames(recording)
    timeout = args.timeout or min(3600, frames // 30 + 180)
    results = []
    for run in range(1, args.runs + 1):
        data, saves = fresh_run(args.seed.resolve(), out / f"replay-{run}")
        report = out / f"replay-{run}.jsonl"
        command = [str(exe), "--background", "--timeout", str(timeout), "--data-dir", str(data), "--save-dir", str(saves),
                   "--replay", str(recording), "--hash-out", str(report)]
        started = time.monotonic()
        try:
            code = subprocess.call(command, timeout=timeout + 60)
        except subprocess.TimeoutExpired:
            code = "timeout"
        elapsed = time.monotonic() - started
        print(f"Replay {run}/{args.runs}: exit {code} after {elapsed:.0f} s")
        results.append({"run": run, "exit": code, "seconds": round(elapsed, 1)})
        if code == MISMATCH:
            print("The replay conditions differ from the recording (see the run's log); stopping.", file=sys.stderr)
            break
    (out / "replays.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    return 0 if all(r["exit"] == 0 for r in results) and len(results) == args.runs else 2


def cmd_compare(args):
    out = args.out.resolve()
    reports = [out / "record.jsonl"] if (out / "record.jsonl").exists() and args.with_record else []
    reports += sorted(out.glob("replay-*.jsonl"), key=lambda p: int(p.stem.split("-")[1]))
    argv = [str(r) for r in reports] + ["--recording", str(out / "session.smrec")]
    return netplay_compare.main(argv)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("seed")
    p.add_argument("--exe", type=Path, required=True)
    p.add_argument("--data", type=Path, required=True)
    p.add_argument("--saves", type=Path)
    p.add_argument("--out", type=Path, required=True)
    p.set_defaults(fn=cmd_seed)
    p = sub.add_parser("record")
    p.add_argument("--exe", type=Path, required=True)
    p.add_argument("--seed", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--hash-every", type=int, default=60)
    p.set_defaults(fn=cmd_record)
    p = sub.add_parser("replay")
    p.add_argument("--seed", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--exe-name", default="OpenShao.exe")
    p.add_argument("--runs", type=int, default=5)
    p.add_argument("--timeout", type=int)
    p.set_defaults(fn=cmd_replay)
    p = sub.add_parser("compare")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--with-record", action="store_true", help="also compare the visible record run")
    p.set_defaults(fn=cmd_compare)
    args = parser.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
