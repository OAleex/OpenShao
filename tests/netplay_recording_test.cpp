#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "netplay/recording.h"

using namespace openshao::netplay;
namespace fs = std::filesystem;
static unsigned failures = 0;
static void Check(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}
static FrameSample Sample(uint32_t frame) {
    FrameSample s{};
    for (unsigned port = 0; port < kPorts; ++port) {
        auto& p = s[port];
        p.connected = port < 2;
        p.buttons = uint16_t((frame % 9000) * 7 + port);
        for (unsigned i = 0; i < 8; ++i) p.analog[i] = uint8_t(frame + i * 31 + port);
        p.lx = int16_t(-32768 + int(frame * 13 % 65536));
        p.ly = int16_t(frame % 2 ? 32767 : -32768);
        p.rx = int16_t(-int(frame % 1000));
        p.ry = int16_t(port * 1000);
    }
    return s;
}
static std::vector<char> ReadAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
static void WriteAll(const fs::path& p, const std::vector<char>& data) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(data.data(), std::streamsize(data.size()));
}
static bool Opens(const fs::path& p, std::string& error) {
    RecordingReader r;
    return r.Open(p, error);
}
int main(int argc, char** argv) {
    const fs::path dir = fs::path(argc > 1 ? argv[1] : ".") / "netplay_recording_test";
    fs::create_directories(dir);
    const char digits[] = "123456789";
    Check(Crc32c(digits, 9) == 0xE3069283u, "CRC32C matches the standard check value");

    FrameSample decoded{};
    uint8_t wire[kFrameBytes];
    Encode(Sample(77), wire);
    Check(Decode(wire, decoded) && decoded == Sample(77), "every pad field survives encoding, including negative axes");
    wire[0] = 2;
    Check(!Decode(wire, decoded), "a sample with an invalid connection flag is rejected");
    Encode(Sample(1), wire);
    wire[kPadBytes + 1] = 1;
    Check(!Decode(wire, decoded), "a sample with a reserved byte set is rejected");
    input::Pad pad = ToPad(Sample(5)[1]);
    Check(FromPad(pad) == Sample(5)[1], "pads convert to samples and back without loss");

    const auto path = dir / "session.smrec";
    const std::string metadata = "{\"format\":1,\"hash_every\":60,\"conditions\":{\"settings_sha256\":\"abc\"}}";
    constexpr uint32_t kFrames = kBlockFrames * 2 + 41;
    {
        RecordingWriter w;
        std::string error;
        Check(w.Open(path, metadata, error), "the writer creates a recording");
        for (uint32_t i = 0; i < kFrames; ++i) Check(w.Append(Sample(i)), "the writer appends frames");
        Check(w.Finish(error) && w.Frames() == kFrames, "the writer completes with a footer");
    }
    {
        RecordingReader r;
        std::string error;
        Check(r.Open(path, error), "a complete recording opens");
        Check(r.Metadata() == metadata && r.Frames() == kFrames, "metadata and frame count round trip");
        bool same = true;
        for (uint32_t i = 0; i < kFrames; ++i) { FrameSample s; same = same && r.Frame(i, s) && s == Sample(i); }
        Check(same, "every frame across three blocks reads back unchanged");
        FrameSample s;
        Check(!r.Frame(kFrames, s), "reading past the last frame fails");
    }
    const auto original = ReadAll(path);
    std::string error;
    {
        auto data = original;
        data[16 + metadata.size() + 4 + 12 + 300] ^= 0x01;  // inside the first block's samples
        WriteAll(dir / "flipped.smrec", data);
        Check(!Opens(dir / "flipped.smrec", error) && error.find("checksum") != std::string::npos,
              "a changed input byte fails the block checksum");
    }
    {
        auto data = original;
        data.resize(data.size() - 16);
        WriteAll(dir / "no-footer.smrec", data);
        Check(!Opens(dir / "no-footer.smrec", error) && error.find("incomplete") != std::string::npos,
              "a recording without its footer is incomplete and refused");
    }
    {
        auto data = original;
        data.resize(data.size() - 40);
        WriteAll(dir / "truncated.smrec", data);
        Check(!Opens(dir / "truncated.smrec", error), "a recording cut inside a block is refused");
    }
    {
        auto data = original;
        data[16] ^= 0x20;
        WriteAll(dir / "metadata.smrec", data);
        Check(!Opens(dir / "metadata.smrec", error) && error.find("header") != std::string::npos,
              "changed metadata fails the header checksum");
    }
    {
        auto data = original;
        data.push_back(0);
        WriteAll(dir / "trailing.smrec", data);
        Check(!Opens(dir / "trailing.smrec", error), "data after the footer is refused");
    }
    {
        // Remove the last block but keep a footer: the chain and frame count no longer match.
        RecordingWriter w;
        Check(w.Open(dir / "short.smrec", metadata, error), "a second recording opens");
        for (uint32_t i = 0; i < kBlockFrames; ++i) w.Append(Sample(i));
        Check(w.Finish(error), "a one-block recording completes");
        auto data = ReadAll(dir / "short.smrec");
        const auto block = 16 + metadata.size() + 4;
        const auto footer = std::vector<char>(original.end() - 16, original.end());
        data.resize(block);
        data.insert(data.end(), footer.begin(), footer.end());
        WriteAll(dir / "short.smrec", data);
        Check(!Opens(dir / "short.smrec", error), "a footer from another recording is refused");
    }
    Check(!Opens(dir / "missing.smrec", error), "a missing file is refused");

    // Readers on other threads see whole published samples, never a mix of two frames.
    FrameInputs inputs;
    inputs.Publish(0, Sample(0));
    std::atomic<bool> stop{false};
    std::atomic<unsigned> torn{0};
    std::thread reader([&] {
        while (!stop) {
            const auto a = inputs.Pad(0), b = inputs.Pad(1);
            if (a.buttons % 7 != 0 || b.buttons % 7 != 1) ++torn;
        }
    });
    for (uint32_t i = 1; i < 20000; ++i) inputs.Publish(i, Sample(i));
    stop = true;
    reader.join();
    Check(torn == 0 && inputs.Frame() == 19999, "published samples are consistent per port");
    Check(inputs.Pad(4).connected == false && inputs.Pad(-1).connected == false, "ports outside 0..3 are disconnected");

    fs::remove_all(dir);
    std::printf("Netplay recording: %u failures\n", failures);
    return failures ? 1 : 0;
}
