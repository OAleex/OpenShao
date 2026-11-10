# Netplay determinism

Online play runs the whole game on each PC and exchanges only controller input.
Both PCs must reach the same state from the same input. This guide covers the tools that record a session, replay it and measure where the game state differs.

The current runtime is not yet deterministic. Guest threads run in parallel and read host clocks.
These tools measure that state. They do not make the game deterministic.

## Frames and input

A frame ends when the game calls `D3DDevice_Swap`.
At that boundary the session publishes the input sample for the next frame.
Every `input::Poll` call in that frame returns the same sample, on every thread.
The sample holds all four ports, including the keyboard and mouse that share a port.

Frame 0 is published before boot.
Outside a session, `input::Poll` reads the live devices as before.

During a session the game ignores the raw keyboard Enter shortcut of the main menu.
Enter still reaches the game through the recorded sample.

## Options

| Option | Effect |
| --- | --- |
| `--record file.smrec` | Records the input sample of every frame. |
| `--replay file.smrec` | Plays the recorded samples instead of the live devices, then closes the game. |
| `--hash-every frames` | Hashes guest memory every given number of frames. A recording stores its cadence. |
| `--hash-out report.jsonl` | Writes the hashes. |

A replay checks the executable, `settings.toml`, the save folder, the Xbox cache and the installed mods against the recording.
It exits with code 60 if any of them differs. Use isolated copies of the data and save folders for each run.
`--timeout` accepts up to 3600 seconds with `--replay`.

## Files

A `.smrec` file has a JSON header, blocks of up to 256 frames and a footer.
Each block and the header have a CRC32C. A file without its footer is incomplete and is refused.

A hash report starts with a header line and ends with a footer line.
Each snapshot line lists one XXH64 hash for every 64 KiB block that has a committed page.
The set of committed pages is part of each block hash.
Blocks are labeled `image`, `virtual`, `kernel` (first physical megabyte), `physical`, `system` or `stack`.
The physical alias at `0xF0000000` is not scanned.

Hashes are taken at the frame boundary while other guest threads keep running.
A difference can therefore come from execution order alone.

## Measuring a session

`tools/netplay_session.py` runs the whole procedure with isolated copies:

```text
python tools/netplay_session.py seed --exe dist/OpenShao.exe --data <data folder> --out <session>/seed
python tools/netplay_session.py record --exe dist/OpenShao.exe --seed <session>/seed --out <session>
python tools/netplay_session.py replay --seed <session>/seed --out <session> --runs 5
python tools/netplay_session.py compare --out <session> --with-record
```

`seed` asks the executable for the configured save folder with `--paths`. Pass `--saves` to choose another one.
`record` copies the executable into the session folder. Replays use that copy.
Replays run hidden and muted.

`tools/netplay_compare.py` reports the first divergent frame, the divergent blocks and whether each region type stays divergent.
It exits with 0 when every report matches, 1 when they differ and 2 for an invalid or incomplete report.
