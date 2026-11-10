#include "pipeline_assets.h"
#include "pipeline_screen_state.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "gpu/texture.h"

namespace openshao::pipelines {
namespace {
std::mutex describer_mutex;
CandidateDescriber describer;
float pipeline_render_scale = 1;
uint64_t description_generation = 0;
std::unordered_set<uint64_t> predicted_keys, observed_keys;
uint32_t first_ready = 0, first_late = 0, first_missing = 0;
bool trace_recipes = false;
uint32_t trace_predicted = 0, trace_observed = 0;
bool trace_catalogued = false;
bool common_screens_warmed = false, library_screens_warmed = false;
std::unordered_set<uint64_t> interface_programs;
uint32_t trace_objects = 0;
thread_local Batch load_batch = 0;
thread_local uint32_t load_objects = 0, load_variants = 0, load_unknown = 0;
thread_local std::chrono::steady_clock::time_point load_start;

template <typename... Fields>
auto PackKey(const Fields&... fields) {
    std::array<char, (sizeof(Fields) + ...)> key;
    char* next = key.data();
    auto append = [&](const auto& value) {
        std::memcpy(next, &value, sizeof(value));
        next += sizeof(value);
    };
    (append(fields), ...);
    return key;
}

auto BufferStrides(const gpu::Assembled& layout) {
    std::array<uint32_t, std::tuple_size_v<decltype(layout.buffers)>> strides;
    for (size_t i = 0; i < strides.size(); ++i) strides[i] = layout.buffers[i].stride;
    return strides;
}

auto DescriptionKey(const PipelineCandidate& c) {
    const auto& s = c.state;
    const auto& a = c.layout;
    const uint64_t program = s.vs ? ProgramHash(s.vs->program) : 0;
    const uint64_t pixel = s.ps ? gpu::Hash(s.ps->def, sizeof(s.ps->def)) : 0;
    return PackKey(s.simple, s.rs, s.tss, s.vs_handle, s.zscale, program, pixel, s.light_enable, s.lights,
        a.regs, a.decode, a.topology, a.strip, a.pretransformed, a.elements, a.buffer_count, BufferStrides(a),
        c.textures, c.shadow, c.shadow_color, c.depth, c.depth_width, c.render_scale, c.count <= 6);
}

using Description = decltype(DescriptionKey(std::declval<const PipelineCandidate&>()));
struct DescriptionHash {
    size_t operator()(const Description& key) const noexcept {
        // Mix complete words of the render state. Equality still compares every byte.
        uint64_t hash = 1469598103934665603ull;
        auto mix = [&](uint64_t word) {
            hash = (hash ^ word) * 1099511628211ull;
            hash ^= hash >> 32;
        };
        size_t offset = 0;
        for (; offset + sizeof(uint64_t) <= key.size(); offset += sizeof(uint64_t)) {
            uint64_t word;
            std::memcpy(&word, key.data() + offset, sizeof(word));
            mix(word);
        }
        uint64_t tail = 0;
        std::memcpy(&tail, key.data() + offset, key.size() - offset);
        mix(tail);
        return static_cast<size_t>(hash);
    }
};
std::unordered_map<Description, Recipe, DescriptionHash> descriptions;
std::deque<Description> description_order;

// Unit prototypes only differ by texture modes: key on material flags, skip the zero arrays.
auto MaterialPassKey(const PipelineCandidate& c, const MaterialVariant& material, uint32_t kind) {
    const auto& s = c.state;
    const auto& a = c.layout;
    const uint64_t program = s.vs ? (s.vs->hash ? s.vs->hash : ProgramHash(s.vs->program)) : 0;
    const uint64_t pixel = s.ps ? gpu::Hash(s.ps->def, sizeof(s.ps->def)) : 0;
    return PackKey(s.vs_handle, s.zscale, program, pixel,
        a.regs, a.decode, a.topology, a.strip, a.pretransformed, a.elements, a.buffer_count, BufferStrides(a),
        c.textures, c.shadow, c.shadow_color, c.depth, c.depth_width, c.render_scale, c.count <= 6,
        kind, material.object_flags, material.object_flags2, material.transparent);
}

using PassKey = decltype(MaterialPassKey(std::declval<const PipelineCandidate&>(), MaterialVariant{}, 0));
struct PassKeyHash {
    size_t operator()(const PassKey& key) const noexcept {
        return std::hash<std::string_view>{}({key.data(), key.size()});
    }
};
std::unordered_map<PassKey, std::vector<Recipe>, PassKeyHash> prepared_passes;
std::deque<PassKey> prepared_order;
size_t prepared_bytes = 0;
thread_local std::unordered_set<PassKey, PassKeyHash> load_prepared;
thread_local uint64_t load_generation = 0;
void ForgetPreparedLoad() { load_prepared.clear(); }
void RememberPreparedLoad(const PassKey& key) {
    if (load_prepared.size() < 1024) load_prepared.insert(key);
}

// Optional, bounded descriptor evidence for comparing metadata prediction with actual draws.
std::string Hex(const void* bytes, size_t count) {
    const auto* p = static_cast<const uint8_t*>(bytes);
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(count * 2, '0');
    for (size_t i = 0; i < count; ++i) {
        out[i * 2] = digits[p[i] >> 4]; out[i * 2 + 1] = digits[p[i] & 15];
    }
    return out;
}
void Trace(const Recipe& r, const char* source) {
    LOGR("asset recipe %s: key=%s vertex=%s", source, Hex(r.key, r.key_size).c_str(),
         Hex(&r.vertex, sizeof(r.vertex)).c_str());
    LOGR("asset recipe %s: pixel=%s", source, Hex(&r.pixel, sizeof(r.pixel)).c_str());
}
bool Predict(Batch batch, const PipelineCandidate& candidate, const gpu::State& state,
             std::vector<Recipe>* prepared = nullptr) {
    Recipe recipe{};
    const auto program = state.vs ? std::span<const uint32_t>(state.vs->program) : std::span<const uint32_t>{};
    if (!DescribeCandidate(candidate, recipe) || !Submit(batch, recipe, program)) return false;
    if (prepared) prepared->push_back(recipe);
    std::lock_guard lock(describer_mutex);
    if (predicted_keys.insert(KeyHash({recipe.key, recipe.key_size})).second && trace_recipes && trace_predicted++ < 1024)
        Trace(recipe, "predicted");
    return true;
}

// Keep pass recipes, not guest addresses, so copies of a model need not re-enumerate.
class PreparedPass {
public:
    PreparedPass(Batch batch, const PipelineCandidate& prototype, const MaterialVariant& material, uint32_t kind)
        : batch_(batch), unknown_before_(load_unknown) {
        {
            std::lock_guard lock(describer_mutex);
            auto scaled = prototype;
            scaled.render_scale = pipeline_render_scale;
            key_ = MaterialPassKey(scaled, material, kind);
            generation_ = description_generation;
            if (load_generation != generation_) { ForgetPreparedLoad(); load_generation = generation_; }
            // Copies are already attached to this batch; resubmitting repeats hashing in gameplay.
            if (batch == load_batch && load_prepared.contains(key_)) { reused_ = true; return; }
            if (auto it = prepared_passes.find(key_); it != prepared_passes.end()) {
                recipes = it->second;
                reused_ = true;
            }
        }
        if (!reused_) return;
        const auto program = prototype.state.vs ? std::span<const uint32_t>(prototype.state.vs->program)
                                                : std::span<const uint32_t>{};
        for (const auto& recipe : recipes) {
            if (Submit(batch, recipe, program)) ++load_variants;
            else ++load_unknown;
        }
        std::lock_guard lock(describer_mutex);
        if (batch == load_batch && generation_ == description_generation && load_unknown == unknown_before_)
            RememberPreparedLoad(key_);
    }
    ~PreparedPass() {
        if (reused_ || recipes.empty() || load_unknown != unknown_before_) return;
        auto less = [](const Recipe& a, const Recipe& b) {
            return a.key_size != b.key_size ? a.key_size < b.key_size
                : std::memcmp(a.key, b.key, a.key_size) < 0;
        };
        std::sort(recipes.begin(), recipes.end(), less);
        recipes.erase(std::unique(recipes.begin(), recipes.end(), [](const Recipe& a, const Recipe& b) {
            return a.key_size == b.key_size && std::memcmp(a.key, b.key, a.key_size) == 0;
        }), recipes.end());
        recipes.shrink_to_fit();
        std::lock_guard lock(describer_mutex);
        if (generation_ != description_generation) return;
        if (batch_ == load_batch) RememberPreparedLoad(key_);
        if (prepared_passes.count(key_)) return;
        prepared_order.push_back(key_);
        auto [it, inserted] = prepared_passes.emplace(key_, std::move(recipes));
        prepared_bytes += 2 * sizeof(PassKey) + it->second.capacity() * sizeof(Recipe);
        // Keep both forms' passes resident across an in-combat model swap.
        while (prepared_order.size() > 512 || prepared_bytes > 4 * 1024 * 1024) {
            auto oldest = prepared_passes.find(prepared_order.front());
            prepared_bytes -= 2 * sizeof(PassKey) +
                oldest->second.capacity() * sizeof(Recipe);
            prepared_passes.erase(oldest);
            prepared_order.pop_front();
        }
    }
    bool Reused() const { return reused_; }
    std::vector<Recipe> recipes;
private:
    Batch batch_;
    PassKey key_;
    uint64_t generation_ = 0;
    uint32_t unknown_before_ = 0;
    bool reused_ = false;
};

bool ActiveDescriber() {
    std::lock_guard lock(describer_mutex);
    return bool(describer);
}
void FinishScreenBatch(Batch batch, const char* name) {
    SealBatch(batch);
    const auto status = WaitBatch(batch, 30000);
    LOGR("asset screen pipelines: %s, unique %u, pending %u, failed %u", name,
         status.total, status.pending, status.failed);
    ReleaseBatch(batch);
}
void PredictScreen(Batch batch, gpu::State& s, uint32_t primitive, bool depth,
                   const gpu::TexInfo* textures, uint32_t count = 4) {
    gpu::Assembled layout;
    if (!gpu::DescribePipelineLayout(s, primitive, layout)) return;
    PipelineCandidate c{s, layout};
    std::copy_n(textures, 4, c.textures);
    c.count = count; c.depth = depth;
    Predict(batch, c, s);
}

bool Valid(uint32_t p, uint32_t bytes) {
    if (p < 0x10000 || !bytes || uint64_t(p) + bytes > 0x100000000ull) return false;
    for (uint64_t at = p & ~4095u; at < uint64_t(p) + bytes; at += 4096)
        if (!mem::IsCommitted(uint32_t(at))) return false;
    return true;
}
uint32_t Read(uint32_t p) { return Valid(p, 4) ? kernel::R32(p) : 0; }

// ShaderManager::SetShader encodes vertex, pixel and paired handles in the high word.
bool ResolveShader(uint32_t logical, uint32_t& vs, uint32_t& ps, unsigned depth = 0) {
    if (depth > 2) return false;
    uint32_t manager = Read(0x63AFB4);
    if (!manager) manager = 0x63AFB8;
    uint32_t id = logical & 0xFFFF, type = logical >> 16;
    if (type == 0) {
        if (id == 0xFFFF) { vs = 0; return true; }
        if (id >= 117) return false;
        const uint32_t table = Read(manager);
        if (!Valid(table, 117 * 16)) return false;
        vs = Read(table + id * 16); return vs != 0;
    }
    if (type == 1) {
        if (id == 0xFFFF) { ps = 0; return true; }
        if (id >= 29) return false;
        const uint32_t table = Read(manager + 4);
        if (!Valid(table, 29 * 8)) return false;
        ps = Read(table + id * 8); return ps != 0;
    }
    if (type != 2 || id >= 31) return false;
    const uint32_t pair = Read(manager + 8) + id * 8;
    return Valid(pair, 8) && ResolveShader(Read(pair), vs, ps, depth + 1) &&
           ResolveShader(Read(pair + 4), vs, ps, depth + 1);
}
bool ShaderSlot(uint32_t slot, uint32_t& vs, uint32_t& ps) {
    return slot < 177 && ResolveShader(Read(0x63A218 + slot * 4), vs, ps);
}
void TraceShaderCatalog() {
    if (!trace_recipes || trace_catalogued) return;
    trace_catalogued = true;
    uint32_t first_vertex = 0;
    for (uint32_t slot = 0; slot < 177; ++slot) {
        uint32_t vs = first_vertex, ps = 0;
        if (!ShaderSlot(slot, vs, ps)) continue;
        gpu::State state;
        if (!d3d::SnapshotPipelineShaders(vs, ps, state)) continue;
        if (!first_vertex) first_vertex = vs;
        LOGR("asset shader slot %u: vertex %08X program %016llx pixel %08X definition %016llx",
             slot, vs, static_cast<unsigned long long>(state.vs ? state.vs->hash : 0), ps,
             static_cast<unsigned long long>(state.ps ? gpu::Hash(state.ps->def, sizeof(state.ps->def)) : 0));
    }
}

bool Texture(uint32_t id, gpu::TexInfo& info, bool& transparent) {
    info = {};
    if (id == ~0u) return true;
    const uint32_t manager = Read(0x637AEC);
    const uint32_t bank_index = id >> 16;
    if (!manager || bank_index >= 1024) return false;
    const uint32_t bank = Read(manager + 0x30) + bank_index * 0x1C;
    if (!Valid(bank, 0x1C) || !(Read(bank) & 255) || !kernel::R16(bank + 2)) return false;
    const uint32_t entry = Read(bank + 0x18) + (id & 0xFFFF) * 0x4C;
    if (!Valid(entry, 0x4C)) return false;
    const uint32_t header = Read(entry + 0x18);
    if (!Valid(header, 20)) return false;
    gpu::TextureDesc d;
    if (!gpu::Describe(header, d)) return false;
    info.kind = d.cube ? gpu::kTexCube : d.depth > 1 ? gpu::kTex3D : gpu::kTex2D;
    info.rect = d.linear;
    info.shadow = gpu::Format(d.format).depth ? (d.format == 0x2C || d.format == 0x30 ? 2 : 1) : 0;
    transparent = Read(entry + 0x1C) != 1;
    return true;
}

gpu::TexInfo SceneAuxiliaryTexture() {
    gpu::TexInfo texture{gpu::kTex2D,0,0,0}, described{};
    const auto shadow=Read(0x63A1D0);
    bool ignored=false;
    if (Valid(shadow,0x188) && Texture(Read(shadow+0x184),described,ignored) && described.kind)
        texture=described;
    return texture;
}
void PredictAsset(Batch batch, PipelineCandidate& candidate, const gpu::State& state,
                  const gpu::TexInfo& auxiliary, std::vector<Recipe>* prepared = nullptr) {
    // 0012C850/00140750 bind the shadow lookup at stage 2; disabled stages share a canonical key.
    for (const auto texture : {gpu::TexInfo{},auxiliary}) {
        candidate.textures[2]=texture;
        if (Predict(batch,candidate,state,prepared)) ++load_variants;
        else ++load_unknown;
    }
}

void Unit(uint32_t object, uint32_t unit, Batch batch, uint32_t source_type) {
    using namespace gpu;
    if (!Valid(unit, 0x180)) { ++load_unknown; return; }
    const uint32_t material = Read(unit + 0x154), primitive = Read(unit + 0x14);
    if (!Valid(material, 0x50) || !Valid(primitive, 0x24)) { ++load_unknown; return; }
    const uint32_t type = Read(primitive + 4), stride = Read(primitive + 12);
    const uint32_t prim = type == 0x100 ? 5 : 6;
    const uint32_t count = type == 0x100 ? Read(primitive + 16) / 3 * 3 :
                           type == 0x200 ? Read(primitive + 16) + 2 : Read(primitive + 20);
    if (!stride || stride > 4096 || count < 3 || count > 4000000) { ++load_unknown; return; }
    TexInfo base{}, extra{};
    bool transparent = false, ignored = false;
    if (!Texture(Read(material + 12), base, transparent)) { ++load_unknown; return; }
    const uint32_t gloss = Read(material + 20), reflection = Read(material + 24);
    if (!Texture(gloss != ~0u ? gloss : reflection, extra, ignored)) { ++load_unknown; return; }
    // 00137450(0,1) binds the auxiliary texture (colour in C4, depth elsewhere); gloss/reflection replace it.
    const bool inherits_texture = gloss == ~0u && reflection == ~0u;
    if (inherits_texture) extra = {kTex2D, 0, 0, 0};
    MaterialVariant material_state;
    const auto auxiliary=SceneAuxiliaryTexture();
    material_state.object_flags = Read(object + 0x48);
    material_state.object_flags2 = Read(object + 0x4C);
    material_state.transparent = transparent || (Read(unit + 4) & 255);
    // Variants never touch matrices or constants: keep zero state per unit.
    State s{};
    auto reset_state = [&] { s.simple = {}; s.rs = {}; s.tss = {}; };


    if (source_type == 11) {
        // Grouped by bone count before 0012BAA0; unrelated to the static scene groups.
        const uint32_t code = Read(object + 0x4C4);
        for (const auto pass : {4u, 0u, 8u, 12u, 16u, 32u, 2u}) {
          for (unsigned lights = 0; lights < (pass == 2 || pass == 32 ? 1u : 4u); ++lights) {
            auto pair = SkinShaders(code, pass, lights);
            uint32_t vs = 0, ps = 0;
            if (!pair.valid || !ShaderSlot(pair.vertex, vs, ps) || !ShaderSlot(pair.pixel, vs, ps)) {
                ++load_unknown; continue;
            }
            reset_state();
            if (!d3d::SnapshotPipelineShaders(vs, ps, s)) { ++load_unknown; continue; }
            s.streams[0] = {1, stride};
            Assembled layout;
            if (!DescribePipelineLayout(s, prim, layout)) { ++load_unknown; continue; }
            PipelineCandidate prototype{s, layout};
            prototype.count = count;
            prototype.textures[0] = base; prototype.textures[1] = extra; prototype.textures[2] = auxiliary;
            const uint32_t kind = 0x10000000u | pass | uint32_t(gloss != ~0u) << 16 |
                uint32_t(reflection != ~0u) << 17;
            PreparedPass prepared{batch, prototype, material_state, kind};
            if (prepared.Reused()) continue;
            const bool shadow = pass == 2;
            for (unsigned stencil = 0; stencil < (shadow ? 1u : 2u); ++stencil) {
                for (unsigned alpha = 0; alpha < (shadow ? 1u : 2u); ++alpha) {
                  for (unsigned kill=0; kill<2; ++kill) {
                   for (unsigned binding=0; binding<(shadow ? 2u : 1u); ++binding) {
                    for (unsigned blend=0; blend<(shadow ? 3u : 1u); ++blend) {
                    auto variant = material_state;
                    // The character's opacity controls blending, independently of texture alpha.
                    variant.transparent = !shadow && alpha != 0;
                    ApplySkinVariant(s, variant, shadow, stencil != 0);
                    SetAssetAlphaKill(s,kill != 0);
                    if (shadow) ApplySkinShadowBlend(s,blend != 0,blend == 2);
                    PipelineCandidate c{s, layout};
                    c.count = count; c.shadow = shadow; c.shadow_color = shadow;
                    c.depth_width=shadow ? 256 : 0;
                    // The shadow callback skips the material setter and inherits the outer base texture.
                    c.textures[0] = shadow && binding ? TexInfo{} : base;
                    if (!shadow) {
                        c.textures[1] = gloss != ~0u ? extra : TexInfo{};
                    }
                    PredictAsset(batch,c,s,auxiliary,&prepared.recipes);
                    }
                   }
                  }
                }
            }
          }
        }
        return;
    }

    // Enumerate the finite shader choices used by the original normal/dynamic wrappers.
    const uint32_t slots[] = {146,147,148,157,158,159,161,162,163,164,167,170,171,174,175,10,23};
    for (uint32_t slot : slots) {
        if (slot == 167 && reflection == ~0u) continue;
        if (slot >= 146 && slot <= 159 && reflection != ~0u) continue;
        const unsigned pixel_variants = slot == 10 ? 2 : 1;
        for (unsigned pixel_variant = 0; pixel_variant < pixel_variants; ++pixel_variant) {
            uint32_t vs = 0, ps = 0;
            if (!ShaderSlot(slot, vs, ps) ||
                (slot == 10 && !ShaderSlot(120 + pixel_variant, vs, ps)) ||
                (slot == 23 && !ShaderSlot(122, vs, ps))) { ++load_unknown; continue; }
            reset_state();
            if (!d3d::SnapshotPipelineShaders(vs, ps, s)) { ++load_unknown; continue; }
            s.streams[0] = {1, stride};  // only presence and stride, never dereferenced
            Assembled layout;
            if (!DescribePipelineLayout(s, prim, layout)) { ++load_unknown; continue; }
            PipelineCandidate prototype{s, layout};
            prototype.count = count;
            prototype.textures[0] = base; prototype.textures[1] = extra; prototype.textures[2] = auxiliary;
            const uint32_t kind = slot | uint32_t(gloss != ~0u) << 16 |
                uint32_t(reflection != ~0u) << 17 | pixel_variant << 24;
            PreparedPass prepared{batch, prototype, material_state, kind};
            if (prepared.Reused()) continue;
            for (unsigned fade = 0; fade < 2; ++fade) {
              const bool alternate = slot >= 157 && slot <= 159;
              const bool projected = slot == 163 || slot == 164;
              const bool receiver = slot == 170 || slot == 171 || slot == 174 || slot == 175;
              for (unsigned blend = 0; blend < (alternate ? 3u : 1u); ++blend) {
               for (unsigned two_sided = 0; two_sided < (alternate || projected ? 2u : 1u); ++two_sided) {
                auto variant = material_state;
                variant.fade = fade != 0;
                variant.transparent |= fade != 0;
                // Object instances may acquire these original render flags after construction.
                if (blend) variant.object_flags = (variant.object_flags & ~0xC00u) | (blend == 1 ? 0x400 : 0x800);
                if (two_sided) variant.object_flags |= 0x80000000u;
                if (receiver) variant.object_flags |= 0x80000000u;
                if (projected) ApplyProjectedMaterialVariant(s, variant);
                else ApplyMaterialVariant(s, variant);
                const bool dynamic = slot == 10 || slot == 23 || alternate || projected || receiver;
                for (unsigned inherited = 0; inherited < (inherits_texture && !projected && !receiver ? 2u : 1u); ++inherited) {
                    // The dynamic wrapper inherits alpha-kill; the ordinary wrapper clears it.
                    for (unsigned kill = 0; kill < (dynamic ? 2u : 1u); ++kill) {
                        SetAssetAlphaKill(s, kill != 0);
                        PipelineCandidate candidate{s, layout};
                        candidate.count = count;
                        candidate.textures[0] = base;
                        candidate.textures[1] = inherited ? TexInfo{kTex2D, 1, 1, 0} : extra;
                        if (receiver) candidate.textures[1]={kTex2D,0,0,0};
                        if (projected) {
                            // 0013CB10 binds projected shadows in stage 1 (and 3 for the paired-light shader).
                            candidate.textures[1] = {kTex2D,1,1,0};
                            if (slot == 164) candidate.textures[3] = candidate.textures[1];
                        }
                        PredictAsset(batch,candidate,s,auxiliary,&prepared.recipes);
                        if (slot >= 157 && slot <= 159 && inherited == 0) {
                            candidate.depth = false;
                            candidate.textures[1] = {};
                            PredictAsset(batch,candidate,s,auxiliary,&prepared.recipes);
                        }
                    }
                }
               }
              }
            }
        }
    }
    // Shadow-buffer shader pair selected by the enclosing pass (logical slots 19 and 136).
    uint32_t vs = 0, ps = 0;
    if (ShaderSlot(19, vs, ps) && ShaderSlot(136, vs, ps)) {
        reset_state();
        if (d3d::SnapshotPipelineShaders(vs, ps, s)) {
            s.streams[0] = {1, stride};
            Assembled layout;
            if (DescribePipelineLayout(s, prim, layout)) {
                auto variant = material_state;
                variant.shadow = true; variant.transparent = false; variant.fade = false;
                ApplyMaterialVariant(s, variant);
                PipelineCandidate candidate{s, layout};
                candidate.count = count; candidate.shadow = true; candidate.depth_width=256;
                candidate.textures[2] = auxiliary;
                PreparedPass prepared{batch, candidate, material_state, 0x20000000u};
                if (!prepared.Reused()) PredictAsset(batch,candidate,s,auxiliary,&prepared.recipes);
            }
        }
    }
}
}

void SetCandidateDescriber(CandidateDescriber callback) {
    std::lock_guard lock(describer_mutex);
    describer = std::move(callback);
    descriptions.clear();
    description_order.clear();
    prepared_passes.clear(); prepared_order.clear(); prepared_bytes = 0;
    ++description_generation;
    if (describer) {
        predicted_keys.clear(); observed_keys.clear(); first_ready = first_late = first_missing = 0;
        trace_recipes = std::getenv("OPENSHAO_ASSET_PIPELINE_TRACE") != nullptr;
        trace_predicted = trace_observed = 0;
        trace_catalogued = false; trace_objects = 0;
        common_screens_warmed = library_screens_warmed = false;
        interface_programs.clear();
    }
}
void ClearCandidateDescriber() { SetCandidateDescriber({}); }
void SetPipelineRenderScale(float scale) {
    std::lock_guard lock(describer_mutex);
    if (pipeline_render_scale!=scale) {
        descriptions.clear(); description_order.clear();
        prepared_passes.clear(); prepared_order.clear(); prepared_bytes = 0;
        ++description_generation;
    }
    pipeline_render_scale=scale;
}
bool DescribeCandidate(const PipelineCandidate& c, Recipe& r) {
    std::lock_guard lock(describer_mutex);
    if (!describer) return false;
    auto scaled=c;
    scaled.render_scale=pipeline_render_scale;
    auto key = DescriptionKey(scaled);
    if (auto it = descriptions.find(key); it != descriptions.end()) { r = it->second; return true; }
    if (!describer(scaled, r)) return false;
    if (descriptions.size() >= 4096) {
        descriptions.erase(description_order.front());
        description_order.pop_front();
    }
    description_order.push_back(key);
    descriptions.emplace(std::move(key), r);
    return true;
}
void TraceObservedAssetRecipe(const Recipe& r, const gpu::State* state) {
    std::lock_guard lock(describer_mutex);
    if (trace_recipes && trace_observed++ < 128) {
        Trace(r, "observed");
        if (state) {
            LOGR("asset recipe observed state: vertex %08X pixel %08X textures %08X/%08X/%08X/%08X",
                 state->vs_handle, state->ps ? state->ps->handle : 0,
                 state->textures[0], state->textures[1], state->textures[2], state->textures[3]);
            if (state->ps) LOGR("asset recipe observed definition: %s", Hex(state->ps->def, sizeof(state->ps->def)).c_str());
        }
    }
}
void ObserveAssetPipeline(uint64_t key, bool compiled_on_draw) {
    std::lock_guard lock(describer_mutex);
    if (!observed_keys.insert(key).second) return;
    if (predicted_keys.count(key)) (compiled_on_draw ? first_late : first_ready)++;
    else first_missing++;
    if (observed_keys.size() <= 64 || observed_keys.size() % 64 == 0)
        LOGR("asset pipeline first-use: ready %u, late %u, not predicted %u, predicted keys %zu",
             first_ready, first_late, first_missing, predicted_keys.size());
}
void ReportAssetPipelines() {
    std::lock_guard lock(describer_mutex);
    LOGR("asset pipeline first-use total: ready %u, late %u, not predicted %u, predicted keys %zu",
         first_ready, first_late, first_missing, predicted_keys.size());
}

void WarmCommonScreenPipelines() {
    if (!ActiveDescriber() || common_screens_warmed) return;
    common_screens_warmed = true;
    const auto batch = BeginBatch(0);
    if (!batch || Status(batch).state == BatchState::Disabled) { ReleaseBatch(batch); return; }
    for (bool textured : {false,true}) {
        gpu::State s{};
        MakeImmediateScreenInputs(s,textured);
        gpu::TexInfo textures[4]{};
        for (bool bound : {false,true}) {
            if (bound && !textured) continue;
            textures[0] = {bound ? gpu::kTex2D : gpu::kTexNone,0,0,0};
            for (bool depth : {false,true}) for (unsigned zen=0; zen<2; ++zen)
                for (unsigned write=0; write<2; ++write) for (unsigned alpha=0; alpha<2; ++alpha) {
                    ApplyImmediateScreenPipelineState(s,textured);
                    s.rs[gpu::kRsZEnable]=zen;
                    s.simple[gpu::kMZWriteEnable/4]=write;
                    s.simple[gpu::kMColorWriteEnable/4]=alpha ? 0x01010101 : 0x00010101;
                    PredictScreen(batch,s,8,depth,textures);
                }
        }
    }
    FinishScreenBatch(batch,"immediate");
}

void WarmInterfaceShader(uint32_t vertex) {
    if (!ActiveDescriber()) return;
    gpu::State s{};
    if (!d3d::SnapshotPipelineShaders(vertex,0,s) || !s.vs || !IsInterfaceShader(*s.vs)) return;
    uint64_t program=ProgramHash(s.vs->program);
    for (const auto& input : s.vs->inputs) {
        const int32_t fields[] = {input.stream,int32_t(input.offset),int32_t(input.type)};
        program=gpu::Hash(fields,sizeof(fields),program);
    }
    program=gpu::Hash(s.vs->stream_stride.data(),sizeof(s.vs->stream_stride),program);
    {
        std::lock_guard lock(describer_mutex);
        if (!interface_programs.insert(program).second) return;
    }
    const auto batch=BeginBatch(0);
    if (!batch || Status(batch).state == BatchState::Disabled) { ReleaseBatch(batch); return; }
    s.streams[0]={1,s.vs->stream_stride[0]};
    gpu::TexInfo textures[4]{{gpu::kTex2D,0,0,0}};
    if (s.vs->stream_stride[0]==24) {
        ApplyFloatingParticlePipelineState(s);
        for (uint32_t primitive : {6u,8u}) for (bool depth : {false,true})
            for (bool kill : {false,true}) {
                SetAssetAlphaKill(s,kill);
                PredictScreen(batch,s,primitive,depth,textures,8);
            }
        FinishScreenBatch(batch,"floating particles");
        return;
    }
    for (uint32_t prim : {6u,8u}) for (bool depth : {false,true})
        for (auto blend : {ScreenBlend::Alpha,ScreenBlend::Additive,ScreenBlend::ReverseSubtract})
            for (auto stencil : {ScreenStencil::Disabled,ScreenStencil::WriteMask,ScreenStencil::TestMask})
                for (uint32_t color : {0u,0x00010101u,0x01010101u}) for (bool kill : {false,true}) {
                    ApplyInterfacePipelineState(s,blend,stencil);
                    // Screen::Render can restore colour writes while retaining the mask state.
                    s.simple[gpu::kMColorWriteEnable/4]=color;
                    SetAssetAlphaKill(s,kill);
                    PredictScreen(batch,s,prim,depth,textures);
                }
    // Particle batches reuse the screen declaration and can inherit the scene's auxiliary stages (00127ED0).
    for (uint32_t prim : {6u,8u}) for (bool depth : {false,true})
        for (auto blend : {ScreenBlend::Alpha,ScreenBlend::Additive,ScreenBlend::ReverseSubtract})
            for (bool stage1 : {false,true}) for (bool stencil : {false,true})
                for (bool alpha : {false,true}) for (bool kill : {false,true}) for (uint32_t mask : {1u,3u,15u}) {
                    ApplyParticlePipelineState(s,blend,stage1,stencil);
                    SetAssetAlphaKill(s,kill);
                    s.simple[gpu::kMColorWriteEnable/4]=alpha ? 0x01010101 : 0x00010101;
                    for (unsigned t=0;t<4;++t) textures[t]={(mask>>t&1) ? gpu::kTex2D : gpu::kTexNone,0,0,0};
                    PredictScreen(batch,s,prim,depth,textures,8);
                }
    FinishScreenBatch(batch,"interface and particles");
}

void WarmShaderLibraryPipelines() {
    if (!ActiveDescriber() || library_screens_warmed) return;
    uint32_t vertex=0x104, single=0, four=0;
    if (!ShaderSlot(139,vertex,single) || !ShaderSlot(138,vertex,four) || !single || !four) return;
    library_screens_warmed=true;
    const auto batch=BeginBatch(0);
    if (!batch || Status(batch).state == BatchState::Disabled) { ReleaseBatch(batch); return; }
    for (bool taps : {false,true}) {
        gpu::State s{};
        if (!d3d::SnapshotPipelineShaders(taps ? 0x404 : 0x104,taps ? four : single,s)) continue;
        MakeFullscreenInputs(s,taps);
        for (bool wide : {false,true}) for (bool depth : {false,true}) for (bool additive : {false,true}) {
            auto textures=FullscreenTextures(taps);
            // Upscaled targets average a console texel per tap (kTexWide).
            if (wide) for (auto& texture : textures) if (texture.kind) texture.flags |= gpu::kTexWide;
            ApplyFullscreenPipelineState(s,taps,additive);
            PredictScreen(batch,s,6,depth,textures.data());
        }
    }
    FinishScreenBatch(batch,"fullscreen effects");
}
void BeginPipelineLoad() {
    if (load_batch) return;
    ForgetPreparedLoad();
    load_objects = load_variants = load_unknown = 0;
    load_start = std::chrono::steady_clock::now();
    load_batch = BeginBatch(CurrentRoom());
}
void CapturePipelineObject(uint32_t object, uint32_t source_type) {
    if (!Valid(object, 0x58)) return;
    WarmShaderLibraryPipelines();
    const uint32_t count = kernel::R16(object + 0x52), units = Read(object + 0x54);
    if (trace_recipes) {
        TraceShaderCatalog();
        if (trace_objects++ < 32) {
            const uint32_t unit = Read(units), primitive = Read(unit + 0x14);
            LOGR("asset object %08X: source %u units %u table %08X unit %08X primitive %08X type %X stride %u flags %08X/%08X",
                 object, source_type, count, units, unit, primitive, Read(primitive + 4), Read(primitive + 12),
                 Read(object + 0x48), Read(object + 0x4C));
        }
    }
    if (!count || count > 4096 || !Valid(units, count * 4)) return;
    const bool temporary = load_batch == 0;
    if (temporary) BeginPipelineLoad();
    if (!load_batch) return;
    ++load_objects;
    for (uint32_t i = 0; i < count; ++i) Unit(object, Read(units + i * 4), load_batch, source_type);
    if (temporary) { SealPipelineLoad(); EndPipelineLoad(); }
}
void SealPipelineLoad() { if (load_batch) SealBatch(load_batch); }
bool PipelineLoadPending() {
    if (!load_batch) return false;
    const auto status = Status(load_batch);
    return status.state == BatchState::Pending;
}
void EndPipelineLoad() {
    if (!load_batch) return;
    const auto status = Status(load_batch);
    const double ms = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-load_start).count();
    if (load_objects) LOGR("asset pipelines: room %02X, objects %u, candidates %u, unique %u, pending %u, failed %u, unsupported %u, coverage %s, %.0f ms",
        CurrentRoom(), load_objects, load_variants, status.total, status.pending, status.failed, load_unknown,
        load_unknown ? "partial" : "supported paths", ms);
    ReleaseBatch(load_batch); load_batch = 0; ForgetPreparedLoad();
}
}
