"""Report completed Present timings and frame intervals in the existing perf window."""


def adapt_backend(backend, replace_once):
    backend = replace_once(backend, "struct Perf {\n",
        "struct Perf {\n"
        "    double frame_interval_max = 0;\n"
        "    uint64_t frame_intervals = 0, frame_over25 = 0, frame_over50 = 0;\n")
    backend = replace_once(backend, "Perf g_perf;\n", """Perf g_perf;
void SamplePerfFrameInterval(bool enabled) {
    static bool measured = false;
    static PerfClock::time_point previous{};
    if (!enabled) { measured = false; return; }
    const auto now = PerfClock::now();
    if (measured) {
        const double ms = std::chrono::duration<double, std::milli>(now - previous).count();
        g_perf.frame_interval_max = std::max(g_perf.frame_interval_max, ms);
        ++g_perf.frame_intervals;
        g_perf.frame_over25 += ms > 25;
        g_perf.frame_over50 += ms > 50;
    }
    previous = now;
    measured = true;
}
""")
    backend = replace_once(backend, "        g_perf = Perf{};\n",
        '        LOGI("perf: frame intervals max %.2f ms, >25 ms %llu, >50 ms %llu (%llu intervals)",\n'
        '             p.frame_interval_max, (unsigned long long)p.frame_over25,\n'
        '             (unsigned long long)p.frame_over50, (unsigned long long)p.frame_intervals);\n'
        "        g_perf = Perf{};\n")
    signature = "void D3D12Backend::Present(const Surface& front) {\n"
    start = backend.index(signature)
    end = backend.index("\n}\n", start) + 3
    original = backend[start:end]
    body = original[len(signature):-2]
    if body.count("    PerfScope perf_present{g_perf.present};\n") != 1:
        raise ValueError("The pinned renderer changed the Present timing scope")
    body = replace_once(body, "    if (diag::Perf()) LogPerf();\n", "")
    scoped = "".join("    " + line if line.strip() else line for line in body.splitlines(keepends=True))
    adapted = (signature + "    {\n" + scoped + "    }\n"
               "    const bool perf_enabled = diag::Perf();\n"
               "    SamplePerfFrameInterval(perf_enabled);\n"
               "    if (perf_enabled) LogPerf();\n}\n")
    return replace_once(backend, original, adapted)
