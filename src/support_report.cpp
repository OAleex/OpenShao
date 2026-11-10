#include "support_report.h"
#include "diagnostic_privacy.h"
#include <miniz.h>
#include <toml++/toml.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
namespace openshao::support {
namespace {
using File = std::unique_ptr<FILE, decltype(&std::fclose)>;
File OpenFile(const fs::path& path, bool write) {
#ifdef _WIN32
    return File(_wfopen(path.c_str(), write ? L"wb" : L"rb"), &std::fclose);
#else
    return File(std::fopen(path.c_str(), write ? "wb" : "rb"), &std::fclose);
#endif
}
uint64_t DumpSize(const fs::path& path) {
    std::error_code ec;
    if(path.empty() || !fs::is_regular_file(path,ec) || fs::is_symlink(path,ec) || path.extension()!=".dmp") return 0;
    const auto size=fs::file_size(path,ec);
    if(ec || size<44 || size>512ull*1024*1024) return 0;
    auto file=OpenFile(path,false);uint32_t header[8]{};
    if(!file || std::fread(header,1,sizeof(header),file.get())!=sizeof(header) || header[0]!=0x504d444d ||
        !header[2] || header[2]>65536 || header[3]<32 || uint64_t(header[3])+uint64_t(header[2])*12>size) return 0;
    return size;
}
fs::path LatestDump(const fs::path& data) {
    if(data.empty()) return {};
    std::error_code ec;fs::path latest;fs::file_time_type newest=fs::file_time_type::min();
    for(const auto& entry:fs::directory_iterator(data/"crashes",ec)) {
        if(!DumpSize(entry.path())) continue;
        const auto time=entry.last_write_time(ec);
        if(!ec && time>newest) { newest=time;latest=entry.path(); }
    }
    return latest;
}
std::string Json(const toml::table& table) { std::ostringstream out; out << toml::json_formatter{table}; return out.str(); }
std::string Read(const fs::path& path, size_t limit) {
    std::error_code ec; if(fs::is_symlink(path,ec)) return "Linked log omitted.\n";
    std::ifstream file(path, std::ios::binary); if (!file) return {};
    file.seekg(0, std::ios::end); const auto length = file.tellg(); if (length < 0) return {};
    const auto size = std::min(uint64_t(length), uint64_t(limit));
    std::string text(size, '\0'); file.seekg(0); file.read(text.data(), size);
    if (uint64_t(length) > limit) {
        const size_t half = limit/2;
        text.resize(half);
        const auto last_line=text.rfind('\n');
        text.resize(last_line==std::string::npos ? 0 : last_line+1);
        text += "\n[Log truncated; final lines follow]\n";
        file.clear(); file.seekg(int64_t(length)-int64_t(half));
        std::string tail(half, '\0'); file.read(tail.data(), half);
        // A cut inside a path or credential loses the prefix needed by the privacy filter.
        const auto first_line=tail.find('\n');
        if(first_line!=std::string::npos) text += tail.substr(first_line+1);
    }
    // Process by line to bound regex stack use on large logs.
    std::string result;
    std::istringstream lines(text); std::string line;
    while (std::getline(lines, line)) {
        if (line.size() > 16384) line = "[Oversized diagnostic line omitted]";
        result += privacy::Redact(line) + "\n";
    }
    return result;
}
toml::table Parse(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || fs::is_symlink(path, ec) || fs::file_size(path, ec) > 1024*1024) return {};
    try { return toml::parse_file(path.string()); } catch (...) { return {}; }
}
toml::table Settings(const fs::path& path) {
    const auto source = Parse(path); toml::table result;
    for (const auto* key : {"renderer", "fps_cap", "voice_language", "language", "display_mode", "aspect_ratio",
            "width", "height", "render_percent", "anisotropic_filtering", "vsync", "bloom_effect", "show_fps", "rumble",
            "restore_dialogue", "cinematic_bars", "scorpion_unfiltered", "improved_difficulty", "master_volume", "music_volume",
            "effects_volume", "speech_volume", "brightness", "deadzone"}) {
        if (auto* node = source.get(key)) {
            if (auto value = node->value<std::string>()) result.insert(key, privacy::Redact(value->substr(0,256)));
            else if (node->is_boolean() || node->is_integer() || node->is_floating_point()) result.insert(key, *node);
        }
    }
    const std::pair<const char*,std::vector<const char*>> groups[] = {
        {"enhancements", {"restore_dialogue","cinematic_bars","scorpion_unfiltered","improved_difficulty"}},
        {"game", {"music_volume","effects_volume","speech_volume","brightness","versus_rounds","versus_time","camera_reverse"}},
        {"controller", {"deadzone","rumble"}}, {"mouse", {"sensitivity","invert_x","invert_y","motion"}}
    };
    for(const auto& [group,keys] : groups) {
        toml::table fields;
        for(const auto* key : keys) if(const auto* node=source[group][key].node())
            if(node->is_boolean() || node->is_integer() || node->is_floating_point()) fields.insert(key,*node);
        if(!fields.empty()) result.insert(group,std::move(fields));
    }
    return result;
}
toml::table Mods(const fs::path& path) {
    const auto source = Parse(path); toml::array entries;
    if (const auto* mods = source["mods"].as_array()) for (const auto& node : *mods) {
        if (entries.size() >= 256) break;
        const auto* item = node.as_table(); if (!item) continue;
        toml::table value;
        for (const auto* key : {"id", "version"})
            if (auto text = (*item)[key].value<std::string>()) value.insert(key, privacy::Redact(text->substr(0,256)));
        value.insert("enabled", (*item)["enabled"].value_or(false)); entries.push_back(std::move(value));
    }
    return toml::table{{"mods", std::move(entries)}};
}
toml::table GraphicsSession(const std::string& name,const std::string& text) {
    std::string api="unknown",device,vulkan_device,d3d12_device;
    std::istringstream lines(text);std::string line;
    const auto after=[](const std::string& line,const char* marker) {
        const auto at=line.find(marker);
        if(at==std::string::npos)return std::string{};
        auto value=line.substr(at+std::strlen(marker),256);
        if(!value.empty() && value.back()=='\r')value.pop_back();
        return value;
    };
    while(std::getline(lines,line)) {
        if(auto value=after(line,"Vulkan device: ");!value.empty())vulkan_device=value.substr(0,value.find(" (driver "));
        if(auto value=after(line,"D3D12 adapter: ");!value.empty())d3d12_device=std::move(value);
        // Device selection alone can precede a failed initialization and fallback.
        if(line.find(" guest, ")!=std::string::npos && line.find(" window, render scale ")!=std::string::npos) {
            if(line.find("vulkan: ")!=std::string::npos){api="vulkan";device=vulkan_device;}
            else if(line.find("d3d12: ")!=std::string::npos){api="d3d12";device=d3d12_device;}
        }
    }
    toml::table result{{"log",name},{"api",api}};
    if(!device.empty())result.insert("device",device);
    return result;
}
}
Report Generate(const Source& source) noexcept {
    fs::path temporary;
    try {
        fs::path folder = source.data.empty() ? fs::temp_directory_path()/"OpenShao"/"reports" : source.data/"reports";
        std::error_code ec;
        fs::create_directories(folder, ec);
        if (ec) { folder = fs::temp_directory_path() / "OpenShao" / "reports"; fs::create_directories(folder); }
        static std::atomic_uint sequence{0};
        const auto now = std::chrono::system_clock::now();
        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        const auto seconds=std::chrono::system_clock::to_time_t(now);std::tm utc{};
#ifdef _WIN32
        gmtime_s(&utc,&seconds);
#else
        gmtime_r(&seconds,&utc);
#endif
        char date[32]{};std::strftime(date,sizeof(date),"%Y%m%d-%H%M%S",&utc);
        const auto zip = folder / ("OpenShao-report-" + std::string(date) + "-" + std::to_string(stamp%1000000) + "-" + std::to_string(sequence++) + ".zip");
        temporary = zip; temporary += ".partial";
        const auto dump=source.dump.empty() && source.kind=="manual" ? LatestDump(source.data) : source.dump;
        const auto dump_size=DumpSize(dump);
        auto dump_file=dump_size ? OpenFile(dump,false) : File(nullptr,&std::fclose);
        const bool include_dump=bool(dump_file);
        toml::table metadata{{"format", 2}, {"version", source.version}, {"kind", source.kind},
            {"reason", privacy::Redact(source.reason)}, {"created_utc_us", stamp},
            {"dump_included",include_dump},
            {"dump_status",include_dump ? "included" : source.dump_status.empty() ? "no_completed_dump_available" : source.dump_status},
#ifdef _WIN32
            {"platform", "Windows"},
#else
            {"platform", "Linux"},
#endif
            {"log_filtering", "Account and computer names, host paths, network addresses and credentials are filtered in text logs."}};
        std::vector<std::pair<std::string,std::string>> files{
            {"report.json", Json(metadata)},
            {"settings.json", Json(source.data.empty() ? toml::table{} : Settings(source.data / "settings.toml"))},
            {"mods.json", Json(source.data.empty() ? toml::table{{"mods",toml::array{}}} : Mods(source.data / "mods" / "installed.toml"))}};
        std::vector<fs::path> logs;
        if (!source.data.empty()) for (const auto& entry : fs::directory_iterator(source.data / "logs", ec)) {
            if (entry.is_regular_file(ec) && !entry.is_symlink(ec) && entry.path().extension() == ".log" &&
                entry.path().filename().string().starts_with("openshao")) logs.push_back(entry.path());
        }
        std::sort(logs.begin(), logs.end(), [](const auto& a,const auto& b) {
            std::error_code aec,bec; return fs::last_write_time(a,aec) > fs::last_write_time(b,bec);
        });
        if (!source.session_log.empty()) { std::erase(logs, source.session_log); logs.insert(logs.begin(), source.session_log); }
        if (logs.size() > 3) logs.resize(3);
        if (logs.empty()) files.emplace_back("session.log", "No session log is available.\n");
        toml::array graphics_sessions;
        for (size_t i=0; i<logs.size(); ++i) {
            const auto name=i==0 ? "session.log" : "previous-"+std::to_string(i)+".log";
            auto contents=Read(logs[i],1024*1024);
            graphics_sessions.push_back(GraphicsSession(name,contents));
            files.emplace_back(name,std::move(contents));
        }
        metadata.insert("graphics_sessions",std::move(graphics_sessions));
        files.front().second=Json(metadata);
        mz_zip_archive archive{};
        // Stream the binary dump without loading it into memory. Wide paths support Unicode data folders.
        auto output=OpenFile(temporary,true);
        if(!output || !mz_zip_writer_init_cfile(&archive,output.get(),0)) throw std::runtime_error("Cannot initialize report archive");
        struct Close { mz_zip_archive* p; ~Close() { if(p->m_pState) mz_zip_writer_end(p); } } close{&archive};
        for (const auto& [name,contents] : files)
            if (!mz_zip_writer_add_mem(&archive, name.c_str(), contents.data(), contents.size(), MZ_BEST_SPEED))
                throw std::runtime_error("Cannot add report diagnostics");
        if(include_dump && !mz_zip_writer_add_cfile(&archive,"crash.dmp",dump_file.get(),dump_size,nullptr,nullptr,0,MZ_BEST_SPEED,nullptr,0,nullptr,0))
            throw std::runtime_error("Cannot add crash dump");
        if(!mz_zip_writer_finalize_archive(&archive)) throw std::runtime_error("Cannot finalize report archive");
        mz_zip_writer_end(&archive);
        if(std::fflush(output.get())) throw std::runtime_error("Cannot write report archive");
        output.reset();
        fs::rename(temporary, zip);
        return {zip, {}, include_dump};
    } catch (const std::exception& error) {
        std::error_code ec; if (!temporary.empty()) fs::remove(temporary, ec);
        return {{}, privacy::Redact(error.what())};
    } catch (...) { return {{}, "Report generation failed"}; }
}
}
