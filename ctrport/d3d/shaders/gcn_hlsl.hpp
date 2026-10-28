// SPDX-License-Identifier: MIT
// C++ port of FrankHUN88453/CTR-NitroFueled-PC 96b4aa5474b1d42b0d9f383349dc091ea9a2d385
// tools/gcn2hlsl.py (MIT, ctrport/d3d/LICENSE.FrankHUN88453): ahead-of-time translation of PS4
// GCN shader binaries to HLSL (Shader Model 5) for the Direct3D 11 backend.
//
// Model: one HLSL invocation == one GCN lane. Every SGPR/VGPR is a `uint`, EXEC/VCC and SGPR
// pairs used as lane masks are per-lane `bool`s. Control flow is rebuilt structurally from the
// (structured) PSSL compiler output. Resources are bound from the Gnm input usage slots.
// The output is the same text the Python translator writes for the same input (checked over
// all shaders of the game by ctr_shaders --compare), so the compiled bytecode is the same too.
#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gcn_decode.hpp"

namespace gcn {

struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// usage slot types (Gnm ShaderInputUsageType)
constexpr int U_RES = 0x00, U_SAMP = 0x01, U_CB = 0x02, U_VB = 0x03;
constexpr int U_FETCH = 0x12, U_RT = 0x13, U_ST = 0x15, U_CBT = 0x16, U_VBT = 0x17, U_EUD = 0x1B;
constexpr int U_IGT = 0x1A;                         // PtrInternalGlobalTable: ES->GS and GS->VS ring descriptors
constexpr int ES_VECS = 8;                          // ES->GS vertex: 32 dwords as float4 e[8]
constexpr int GSO_LEN = 64;                         // GS output vertex dwords (GSVS ring attributes)
constexpr int GS_OUT_SCALARS = 4 * (1 + 10);        // D3D11: maxvertexcount * output scalars <= 1024
constexpr int VARYING_FIRST = 15, VARYING_END = 25; // interpolant semantics of VS exports / PS inputs

// Gnm shader struct contents the translator reads (export_shaders.py parse_gnm).
struct Slot {
    int usage, api, reg, flags;
};
struct Input {
    int sem = 0, vgpr = 0, size = 0;               // VS / ES
    int def = 0, flat = 0, linear = 0, custom = 0; // PS
};
struct ExportSlot {
    int sem, out;
};
struct GnmMeta {
    uint32_t shader_size = 0;
    std::map<std::string, uint32_t> regs;
    std::vector<Slot> slots;
    std::vector<Input> inputs;
    std::vector<ExportSlot> exports;
    std::shared_ptr<GnmMeta> copy; // GS: the embedded copy VS
};

struct Ring {
    std::string role; // "", "es", "gs", "copy"
    int64_t max_vert = 0;
    std::string copy_fn;
    int64_t out_verts = 0;
};

inline std::string u(int64_t v) {
    return std::to_string((uint64_t)(v & 0xFFFFFFFF)) + "u";
}
inline std::string num(int64_t v) {
    return std::to_string(v);
}
inline std::string hex8(int64_t v) {
    char b[16];
    snprintf(b, sizeof(b), "%08x", (unsigned)(v & 0xFFFFFFFF));
    return b;
}
inline std::string f32(const std::string& x) {
    return "asfloat(" + x + ")";
}
inline const char* XYZW = "xyzw";
inline std::string comp(int64_t c) {
    return std::string(1, XYZW[c]);
}
inline bool starts(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
}
inline bool ends(const std::string& s, const char* p) {
    const size_t n = strlen(p);
    return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
}
inline bool contains(const std::string& s, const char* p) {
    return s.find(p) != std::string::npos;
}
inline std::vector<std::string> split(const std::string& s, char c) {
    std::vector<std::string> out;
    size_t a = 0;
    for (;;) {
        const size_t b = s.find(c, a);
        out.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) return out;
        a = b + 1;
    }
}
inline std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string r;
    for (size_t k = 0; k < v.size(); k++) r += (k ? sep : "") + v[k];
    return r;
}
// Python re `(?<![\w.])<needle>` anywhere in s
inline bool word_match(const std::string& s, const std::string& needle) {
    for (size_t p = s.find(needle); p != std::string::npos; p = s.find(needle, p + 1)) {
        if (p == 0) return true;
        const char c = s[p - 1];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.')) return true;
    }
    return false;
}

inline const char* COMMON_HLSL = R"(
float cubeid(float x, float y, float z) {
    float ax = abs(x), ay = abs(y), az = abs(z);
    if (az >= ax && az >= ay) return z < 0 ? 5.0 : 4.0;
    if (ay >= ax) return y < 0 ? 3.0 : 2.0;
    return x < 0 ? 1.0 : 0.0;
}
float cubesc(float x, float y, float z) {
    float ax = abs(x), ay = abs(y), az = abs(z);
    if (az >= ax && az >= ay) return z < 0 ? -x : x;
    if (ay >= ax) return x;
    return x < 0 ? z : -z;
}
float cubetc(float x, float y, float z) {
    float ax = abs(x), ay = abs(y), az = abs(z);
    if (az >= ax && az >= ay) return -y;
    if (ay >= ax) return y < 0 ? -z : z;
    return -y;
}
float cubema(float x, float y, float z) {
    float ax = abs(x), ay = abs(y), az = abs(z);
    if (az >= ax && az >= ay) return 2.0 * z;
    if (ay >= ax) return 2.0 * y;
    return 2.0 * x;
}
bool cmp_class(float v, uint m) {
    uint u = asuint(v);
    bool neg = (u >> 31) != 0;
    uint e = (u >> 23) & 0xFFu, f = u & 0x7FFFFFu;
    bool r = false;
    if (e == 0xFFu && f != 0) r = r || ((m & 3u) != 0);
    if (e == 0xFFu && f == 0) r = r || ((m & (neg ? 4u : 512u)) != 0);
    if (e != 0 && e != 0xFFu) r = r || ((m & (neg ? 8u : 256u)) != 0);
    if (e == 0 && f != 0) r = r || ((m & (neg ? 16u : 128u)) != 0);
    if (e == 0 && f == 0) r = r || ((m & (neg ? 32u : 64u)) != 0);
    return r;
}
float quad_swizzle(float v, uint perm, uint lane) {
    float dx = ddx_fine(v), dy = ddy_fine(v);
    uint src = (perm >> (lane * 2u)) & 3u;
    float r = v;
    if ((src ^ lane) & 1u) r += (lane & 1u) ? -dx : dx;
    if ((src ^ lane) & 2u) r += (lane & 2u) ? -dy : dy;
    return r;
}
)";

inline std::string vsout_struct() {
    std::string s = "struct VSOut {\n    float4 pos : SV_Position;\n";
    for (int v = VARYING_FIRST; v < VARYING_END; v++) s += "    float4 a" + num(v) + " : VARYING" + num(v) + ";\n";
    return s + "};";
}
inline std::string esout_struct() {
    return "struct ESOut {\n    float4 e[" + num(ES_VECS) + "] : ESGS0;\n};";
}

struct Tag {
    std::string kind; // T, B, S, CB, V, P, RING
    int64_t a = 0, b = 0;
};
constexpr int64_t kNone = INT64_MIN; // Python None in lane-spill keys

class Translator {
public:
    std::string kind;
    Ring ring;
    const std::vector<Inst>& insts;
    const GnmMeta& meta;
    int64_t code_len;
    std::vector<std::string> lines;
    int ind = 1;
    std::set<int64_t> sregs, vregs, masks;
    std::vector<std::pair<int64_t, std::pair<int, std::string>>> textures; // slot -> (dims, use), insertion order
    std::map<int64_t, std::string> samplers;
    std::set<int64_t> cbuffers;
    std::map<int64_t, std::string> inputs_used;
    std::set<int64_t> outputs;
    bool depth_out = false;
    std::set<std::pair<int64_t, int64_t>> lane_spills;
    bool uses_quad = false, uses_swizzle = false, uses_vin = false, uses_scale = false;
    std::set<int64_t> cube_face_vgprs, lane_addr_vgprs, lds_slots;
    std::map<int64_t, int> force_dims;
    std::map<int64_t, std::set<int>> dims_seen;
    // slots read through cube face coordinates, and slots read any other way: a slot only in the
    // first set is declared TextureCube (dims 4), so filtering and mip selection cross face edges
    std::set<int64_t> cube_slots, plain_slots;
    bool exec_written = false;
    std::map<int64_t, size_t> idx;
    std::map<int64_t, Tag> tags;
    std::set<int64_t> mask_pairs;
    int loop_id = 0;

    Translator(std::string kind_, const std::vector<Inst>& insts_, const GnmMeta& meta_, int64_t code_len_,
               std::map<int64_t, int> force = {}, Ring ring_ = {})
        : kind(std::move(kind_)), ring(std::move(ring_)), insts(insts_), meta(meta_), code_len(code_len_),
          force_dims(std::move(force)) {
        for (const auto& i : insts)
            if (writes_exec(i)) exec_written = true;
        for (size_t k = 0; k < insts.size(); k++) idx[insts[k].pc] = k;
        for (const auto& s : meta.slots) {
            if (s.reg >= 16) continue; // lives in extended user data, loaded via SMRD
            tag_slot(s.usage, s.api, s.reg, s.flags);
        }
        find_masks();
    }

    // ------------------------------------------------------------ textures (ordered dict)
    std::pair<int, std::string>* texture(int64_t slot) {
        for (auto& t : textures)
            if (t.first == slot) return &t.second;
        return nullptr;
    }
    void set_texture(int64_t slot, int dims, const char* use, bool cube = false) {
        (cube ? cube_slots : plain_slots).insert(slot);
        if (auto* t = texture(slot))
            *t = {dims, use};
        else
            textures.push_back({slot, {dims, use}});
    }

    // ------------------------------------------------------------ analysis
    static bool writes_exec(const Inst& i) {
        if (starts(i.name, "v_cmpx")) return true;
        if (ends(i.name, "saveexec_b64")) return true;
        if ((i.fmt == "SOP1" || i.fmt == "SOP2" || i.fmt == "SOPK") && i.geti("sdst") == 126) return true;
        return false;
    }
    void tag_slot(int u, int api, int64_t reg, int fl) {
        if (u == U_RES) {
            const int n = (fl & 1) ? 8 : 4;
            for (int k = 0; k < n; k++) tags[reg + k] = {n == 8 ? "T" : "B", api, k};
        } else if (u == U_SAMP) {
            for (int k = 0; k < 4; k++) tags[reg + k] = {"S", api, k};
        } else if (u == U_CB) {
            for (int k = 0; k < 4; k++) tags[reg + k] = {"CB", api, k};
        } else if (u == U_VB) {
            for (int k = 0; k < 4; k++) tags[reg + k] = {"V", api, k};
        } else if (u == U_FETCH || u == U_RT || u == U_ST || u == U_CBT || u == U_VBT || u == U_EUD || u == U_IGT) {
            tags[reg] = {"P", u, 0};
            tags[reg + 1] = {"P", u, 1};
        }
    }
    const Tag* tag(int64_t r) const {
        auto it = tags.find(r);
        return it == tags.end() ? nullptr : &it->second;
    }
    void find_masks() {
        std::set<int64_t> m;
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& i : insts) {
                std::optional<int64_t> d;
                if (ends(i.name, "saveexec_b64")) {
                    d = i.I("sdst");
                } else if (i.fmt == "VOP3" && i.S("base") == "VOPC" && i.I("sdst") < 104) {
                    d = i.I("sdst");
                } else if ((i.fmt == "SOP1" || i.fmt == "SOP2") && ends(i.name, "_b64") && i.I("sdst") < 104) {
                    for (const char* k : {"ssrc0", "ssrc1"}) {
                        const Operand* o = i.opt(k);
                        if (o &&
                            ((o->kind == Operand::Special && (o->special == "exec_lo" || o->special == "vcc_lo")) ||
                             (o->kind == Operand::S && m.count(o->value))))
                            d = i.I("sdst");
                    }
                }
                if (d && !m.count(*d)) {
                    m.insert(*d);
                    changed = true;
                }
            }
        }
        mask_pairs = m;
    }

    // ------------------------------------------------------------ emit helpers
    std::string indent() const { return std::string(4 * ind, ' '); }
    void emit(const std::string& s) {
        lines.push_back(indent() + s);
        // VCC doubles as a plain scalar register: keep its 32-bit value in sync
        if (word_match(s, "vcc = ") && !contains(s, "vccl = "))
            lines.push_back(indent() + "vccl = vcc ? 0xFFFFFFFFu : 0u;");
    }
    std::string S(int64_t n) {
        if (n == 106) return "vcc_lo_";
        if (n == 107) return "vcc_hi_";
        if (n == 124) return "m0";
        if (n == 126) return "exec_lo_";
        if (n == 127) return "exec_hi_";
        if (n >= 104) throw Unsupported("sgpr" + num(n));
        sregs.insert(n);
        return "s" + num(n);
    }
    std::string V(int64_t n) {
        vregs.insert(n);
        return "v" + num(n);
    }
    std::string M(int64_t n) {
        if (n == 106) return "vcc";
        if (n == 126) return "ex";
        masks.insert(n);
        return "k" + num(n);
    }
    std::string src(const Operand& o) {
        switch (o.kind) {
        case Operand::S: return S(o.value);
        case Operand::V: return V(o.value);
        case Operand::Lit:
            if (o.none) throw Unsupported("literal operand without a literal");
            return "0x" + hex8(o.value) + "u";
        case Operand::Int: return u(o.value);
        case Operand::Float: return std::string("asuint(") + o.fstr + "f)";
        default: break;
        }
        const std::string& v = o.special;
        if (v == "vcc_lo") return "vccl";
        if (v == "exec_lo") return "(ex ? 0xFFFFFFFFu : 0u)";
        if (v == "vcc_hi") return "vcch";
        if (v == "exec_hi") return "0u";
        if (v == "m0") return "m0";
        if (v == "scc") return "(scc ? 1u : 0u)";
        if (v == "vccz") return "(vcc ? 0u : 1u)";
        if (v == "execz") return "(ex ? 0u : 1u)";
        throw Unsupported("operand " + v);
    }
    std::string fsrc(const Inst& i, int n) {
        const Operand* o = i.opt("src" + num(n));
        if (!o) throw Unsupported("missing src");
        std::string e = f32(src(*o));
        if (i.fmt == "VOP3") {
            if ((i.geti("abs").value_or(0) >> n) & 1) e = "abs(" + e + ")";
            if ((i.geti("neg").value_or(0) >> n) & 1) e = "(-" + e + ")";
        }
        return e;
    }
    std::string vsrc1(const Inst& i) { return V(i.I("vsrc1")); }

    void write_v(const Inst& i, int64_t d, std::string expr, bool is_float) {
        if (is_float && i.fmt == "VOP3") {
            const int64_t om = i.geti("omod").value_or(0);
            if (om == 1)
                expr = "((" + expr + ") * 2.0f)";
            else if (om == 2)
                expr = "((" + expr + ") * 4.0f)";
            else if (om == 3)
                expr = "((" + expr + ") * 0.5f)";
            if (i.geti("clamp").value_or(0)) expr = "saturate(" + expr + ")";
        }
        const std::string rhs = is_float ? "asuint(" + expr + ")" : expr;
        const std::string guard = exec_written ? "if (ex) " : "";
        emit(guard + V(d) + " = " + rhs + ";");
        if (i.name == "v_cubeid_f32")
            cube_face_vgprs.insert(d);
        else
            cube_face_vgprs.erase(d);
        // lane-id derived addresses (v_mbcnt, shifted): the shader keeps private spills in LDS
        std::vector<int64_t> srcs;
        for (const char* k : {"src0", "src1"})
            if (const Operand* o = i.opt(k); o && o->kind == Operand::V) srcs.push_back(o->value);
        if (auto v1 = i.geti("vsrc1")) srcs.push_back(*v1);
        bool lane = starts(i.name, "v_mbcnt");
        if (!lane && (i.name == "v_lshlrev_b32" || i.name == "v_lshl_b32" || i.name == "v_mbcnt_hi_u32_b32"))
            for (int64_t r : srcs)
                if (lane_addr_vgprs.count(r)) lane = true;
        if (lane)
            lane_addr_vgprs.insert(d);
        else
            lane_addr_vgprs.erase(d);
    }
    void write_s(int64_t d, const std::string& expr) {
        if (d == 126)
            emit("ex = (" + expr + ") != 0u;");
        else if (d == 106)
            emit("vccl = " + expr + "; vcc = vccl != 0u;");
        else if (d == 107)
            emit("vcch = " + expr + ";");
        else if (d == 127) {
        } else
            emit(S(d) + " = " + expr + ";");
        if (d < 104) tags.erase(d);
    }
    void write_mask(int64_t d, const std::string& expr) {
        if (d == 126)
            emit("ex = " + expr + ";");
        else if (d == 106)
            emit("vcc = " + expr + ";");
        else
            emit(M(d) + " = " + expr + ";");
    }
    std::string mask_of(const Operand& o) {
        if (o.kind == Operand::Special) {
            if (o.special == "exec_lo") return "ex";
            if (o.special == "vcc_lo") return "vcc";
            throw Unsupported("mask operand " + o.special);
        }
        if (o.kind == Operand::S) {
            if (mask_pairs.count(o.value)) return M(o.value);
            return "(" + S(o.value) + " != 0u)";
        }
        if (o.kind == Operand::Int) return o.value != 0 ? "true" : "false";
        if (o.kind == Operand::Lit) return (o.none || o.value != 0) ? "true" : "false";
        throw Unsupported("mask operand");
    }

    // ------------------------------------------------------------ control flow
    int64_t branch_target(const Inst& i) const {
        const int64_t off = (int64_t)((i.I("simm16") ^ 0x8000)) - 0x8000;
        return i.pc + 4 + off * 4;
    }
    std::optional<std::string> cond(const Inst& i) const {
        const std::string& n = i.name;
        if (n == "s_cbranch_scc0") return "!scc";
        if (n == "s_cbranch_scc1") return "scc";
        if (n == "s_cbranch_vccz") return "!vcc";
        if (n == "s_cbranch_vccnz") return "vcc";
        if (n == "s_cbranch_execz") return "!ex";
        if (n == "s_cbranch_execnz") return "ex";
        if (n == "s_branch") return "true";
        return std::nullopt;
    }
    bool is_branch(const Inst& i) const { return i.name == "s_branch" || (starts(i.name, "s_cbranch") && cond(i)); }
    size_t tidx(int64_t pc) const {
        auto it = idx.find(pc);
        if (it == idx.end()) {
            if (pc >= code_len) return insts.size();
            throw Unsupported("branch into instruction");
        }
        return it->second;
    }
    std::optional<size_t> loop_end_for(size_t k) const {
        std::optional<size_t> last;
        for (size_t j = k; j < insts.size(); j++) {
            const Inst& i = insts[j];
            if (is_branch(i) && tidx(branch_target(i)) == k) last = j;
        }
        return last;
    }
    using Loop = std::optional<std::pair<size_t, size_t>>;
    void emit_range(size_t a, size_t b, Loop loop = std::nullopt) {
        size_t k = a;
        while (k < b) {
            const auto le = loop_end_for(k);
            if (le && *le < b && (!loop || k != loop->first)) {
                // iteration cap: bad data must never hang the GPU (TDR)
                const int64_t cap = kind == "gs" ? ring.out_verts + 1 : 4096;
                loop_id += 1;
                const std::string li = "li" + num(loop_id) + "_";
                emit("[loop] for (uint " + li + " = 0u; " + li + " < " + num(cap) + "u; " + li + "++) {");
                ind += 1;
                emit_range(k, *le, std::make_pair(k, *le + 1));
                const Inst& br = insts[*le];
                const std::string c = *cond(br);
                if (c != "true") emit("if (!(" + c + ")) break;");
                ind -= 1;
                emit("}");
                k = *le + 1;
                continue;
            }
            const Inst& i = insts[k];
            if (is_branch(i)) {
                const size_t t = tidx(branch_target(i));
                const std::string c = *cond(i);
                if (loop && t == loop->second) {
                    emit(c == "true" ? "break;" : "if (" + c + ") break;");
                    k += 1;
                    continue;
                }
                if (loop && t == loop->first) {
                    emit(c == "true" ? "continue;" : "if (" + c + ") continue;");
                    k += 1;
                    continue;
                }
                if (t <= k) throw Unsupported("unstructured backward branch");
                if (t > b) {
                    if (kind == "ps" && is_kill_block(t)) {
                        emit(c != "true" ? "if (" + c + ") discard;" : "discard;");
                        k += 1;
                        continue;
                    }
                    if (t >= insts.size() - 1 && insts.back().name == "s_endpgm") {
                        emit_exit(c);
                        k += 1;
                        continue;
                    }
                    throw Unsupported("branch out of region");
                }
                if (c == "true") throw Unsupported("unconditional forward jump");
                // if / else
                const Inst* last = t - 1 > k ? &insts[t - 1] : nullptr;
                if (last && last->name == "s_branch") {
                    const size_t e = tidx(branch_target(*last));
                    if (t < e && e <= b && !(loop && e == loop->second)) {
                        emit("if (!(" + c + ")) {");
                        ind += 1;
                        emit_range(k + 1, t - 1, loop);
                        ind -= 1;
                        emit("} else {");
                        ind += 1;
                        emit_range(t, e, loop);
                        ind -= 1;
                        emit("}");
                        k = e;
                        continue;
                    }
                }
                emit("if (!(" + c + ")) {");
                ind += 1;
                emit_range(k + 1, t, loop);
                ind -= 1;
                emit("}");
                k = t;
                continue;
            }
            inst(i, k);
            k += 1;
        }
    }
    // `s_mov_b64 exec, 0; exp null...; s_endpgm` tail emitted by the PSSL compiler for kill
    bool is_kill_block(size_t t) {
        if (t >= insts.size()) return false;
        const Inst& i = insts[t];
        if (i.name != "s_mov_b64" || i.geti("sdst") != 126 || src(i.O("ssrc0")) != "0u") return false;
        for (size_t j = t + 1; j < insts.size(); j++) {
            if (insts[j].name == "s_endpgm") return true;
            if (insts[j].fmt != "EXP" || insts[j].geti("en").value_or(0) != 0) return false;
        }
        return false;
    }
    void emit_exit(const std::string& c) {
        if (kind == "gs") {
            emit(c != "true" ? "if (" + c + ") return;" : "return;");
            return;
        }
        if (kind == "ps")
            emit(c != "true" ? "if (" + c + ") { if (!ex) discard; return o; }" : "{ if (!ex) discard; return o; }");
        else
            emit(c != "true" ? "if (" + c + ") return o;" : "return o;");
    }

    // ------------------------------------------------------------ instructions
    void inst(const Inst& i, size_t k) {
        const std::string& n = i.name;
        if (n == "s_sendmsg") return op_s_sendmsg(i);
        if (n == "s_swappc_b64") return op_s_swappc_b64(i);
        if (n == "s_cbranch_join") throw Unsupported("s_cbranch_join");
        if (i.fmt == "VOP1" || i.fmt == "VOP2" || i.fmt == "VOP3" || i.fmt == "VOPC") return valu(i, k);
        if (i.fmt == "SOP1" || i.fmt == "SOP2" || i.fmt == "SOPC" || i.fmt == "SOPK") return salu(i);
        for (const char* q : {"s_waitcnt", "s_nop", "s_dcache_inv", "s_sendmsg", "s_setprio", "s_barrier",
                              "s_icache_inv", "s_sleep", "s_trap"})
            if (n == q) return;
        if (n == "s_endpgm") {
            if (!insts.empty() && k != insts.size() - 1) emit_exit("true");
            return;
        }
        if (i.fmt == "SMRD") return smrd(i);
        if (i.fmt == "MTBUF" || i.fmt == "MUBUF") {
            const Tag* t = tag(i.I("srsrc"));
            if (t && t->kind == "RING") return ring_access(i);
            return buffer_load(i);
        }
        if (i.fmt == "MIMG") return mimg(i);
        if (i.fmt == "EXP") return exp(i);
        if (i.fmt == "VINTRP") return vintrp(i, k);
        if (n == "ds_write_b32" || n == "ds_read_b32") return ds_private(i);
        if (n == "ds_swizzle_b32") return ds_swizzle(i, k);
        throw Unsupported(n);
    }

    // ---- scalar
    void salu(const Inst& i) {
        const std::string& n = i.name;
        if (i.fmt == "SOPK") {
            const int64_t k = i.I("simm16");
            const int64_t ks = (k & 0x8000) ? k - 0x10000 : k;
            const int64_t d = i.I("sdst");
            if (n == "s_movk_i32") return write_s(d, u(ks));
            if (n == "s_addk_i32") return write_s(d, S(d) + " + " + u(ks));
            if (n == "s_mulk_i32") return write_s(d, "uint(int(" + S(d) + ") * " + num(ks) + ")");
            if (starts(n, "s_cmpk_")) {
                const std::string op = cmp_op(split(n, '_')[2]);
                if (ends(n, "i32")) return emit("scc = int(" + S(d) + ") " + op + " " + num(ks) + ";");
                return emit("scc = " + S(d) + " " + op + " " + num(k) + "u;");
            }
            throw Unsupported(n);
        }
        if (i.fmt == "SOPC") {
            const std::string a = src(i.O("ssrc0")), b = src(i.O("ssrc1"));
            const auto parts = split(n, '_');
            if (parts.size() > 1 && parts[1] == "cmp") {
                const std::string op = cmp_op(parts.at(2));
                if (parts.at(3) == "i32") return emit("scc = int(" + a + ") " + op + " int(" + b + ");");
                return emit("scc = " + a + " " + op + " " + b + ";");
            }
            if (n == "s_bitcmp0_b32") return emit("scc = ((" + a + " >> (" + b + " & 31u)) & 1u) == 0u;");
            if (n == "s_bitcmp1_b32") return emit("scc = ((" + a + " >> (" + b + " & 31u)) & 1u) != 0u;");
            throw Unsupported(n);
        }
        const int64_t d = i.geti("sdst").value_or(-1);
        if (ends(n, "_b64") || ends(n, "_i64") || ends(n, "_u64")) return salu64(i);
        if (i.fmt == "SOP1") {
            const std::string a = src(i.O("ssrc0"));
            if (n == "s_mov_b32") {
                const Operand& o = i.O("ssrc0");
                if (o.kind == Operand::S && tags.count(o.value) && d < 104) {
                    tags[d] = tags[o.value];
                    emit(S(d) + " = " + a + ";");
                    return;
                }
                return write_s(d, a);
            }
            if (n == "s_not_b32") {
                write_s(d, "~" + a);
                if (d < 104) emit("scc = " + S(d) + " != 0u;");
                return;
            }
            if (n == "s_brev_b32") return write_s(d, "reversebits(" + a + ")");
            if (n == "s_bcnt1_i32_b32") return write_s(d, "countbits(" + a + ")");
            if (n == "s_ff1_i32_b32") return write_s(d, "firstbitlow(" + a + ")");
            if (n == "s_flbit_i32_b32")
                return write_s(d, "(firstbithigh(" + a + ") == 0xFFFFFFFFu ? 0xFFFFFFFFu : 31u - firstbithigh(" + a +
                                      "))");
            if (n == "s_sext_i32_i8") return write_s(d, "uint(int(" + a + " << 24) >> 24)");
            if (n == "s_sext_i32_i16") return write_s(d, "uint(int(" + a + " << 16) >> 16)");
            if (n == "s_abs_i32") return write_s(d, "uint(abs(int(" + a + ")))");
            if (n == "s_cmov_b32") return emit("if (scc) " + S(d) + " = " + a + ";");
            throw Unsupported(n);
        }
        const std::string a = src(i.O("ssrc0")), b = src(i.O("ssrc1"));
        struct OpDef {
            const char* name;
            const char* op;
            int carry; // 1 True, 0 False, -1 None
        };
        static const OpDef ops[] = {{"s_add_u32", "+", 1}, {"s_sub_u32", "-", 1}, {"s_add_i32", "+", 1},
                                    {"s_sub_i32", "-", 1}, {"s_and_b32", "&", 0}, {"s_or_b32", "|", 0},
                                    {"s_xor_b32", "^", 0}, {"s_mul_i32", "*", -1}};
        for (const auto& o : ops) {
            if (n != o.name) continue;
            const std::string tmp = "(" + a + " " + o.op + " " + b + ")";
            if (n == "s_add_u32")
                emit("scc = (" + a + " + " + b + ") < " + a + ";");
            else if (n == "s_sub_u32")
                emit("scc = " + b + " > " + a + ";");
            write_s(d, tmp);
            if (o.carry == 0 && d < 104) emit("scc = " + S(d) + " != 0u;");
            return;
        }
        if (n == "s_lshl_b32" || n == "s_lshr_b32") {
            write_s(d, "(" + a + " " + (contains(n, "lshl") ? "<<" : ">>") + " (" + b + " & 31u))");
            if (d < 104) emit("scc = " + S(d) + " != 0u;");
            return;
        }
        if (n == "s_ashr_i32") return write_s(d, "uint(int(" + a + ") >> (" + b + " & 31u))");
        if (n == "s_andn2_b32") return write_s(d, "(" + a + " & ~" + b + ")");
        if (n == "s_orn2_b32") return write_s(d, "(" + a + " | ~" + b + ")");
        if (n == "s_cselect_b32") return write_s(d, "(scc ? " + a + " : " + b + ")");
        if (n == "s_min_u32" || n == "s_max_u32") return write_s(d, n.substr(2, 3) + "(" + a + ", " + b + ")");
        if (n == "s_min_i32" || n == "s_max_i32")
            return write_s(d, "uint(" + n.substr(2, 3) + "(int(" + a + "), int(" + b + ")))");
        if (n == "s_bfm_b32") return write_s(d, "(((1u << (" + a + " & 31u)) - 1u) << (" + b + " & 31u))");
        if (n == "s_bfe_u32")
            return write_s(d, "((" + a + " >> (" + b + " & 31u)) & ((1u << ((" + b + " >> 16) & 127u)) - 1u))");
        if (n == "s_bfe_i32")
            return write_s(d, "uint(int(" + a + " << (32u - ((" + b + " >> 16) & 127u) - (" + b +
                                  " & 31u))) >> (32u - ((" + b + " >> 16) & 127u)))");
        throw Unsupported(n);
    }
    static std::string cmp_op(const std::string& c) {
        if (c == "eq") return "==";
        if (c == "lg") return "!=";
        if (c == "gt") return ">";
        if (c == "ge") return ">=";
        if (c == "lt") return "<";
        if (c == "le") return "<=";
        throw Unsupported("compare " + c);
    }
    void salu64(const Inst& i) {
        const std::string& n = i.name;
        const int64_t d = i.I("sdst");
        const bool is_mask = d == 106 || d == 126 || mask_pairs.count(d);
        if (n == "s_mov_b64") {
            if (is_mask) return write_mask(d, mask_of(i.O("ssrc0")));
            const Operand& o = i.O("ssrc0");
            if (o.kind == Operand::S) {
                for (int k = 0; k < 2; k++)
                    if (tags.count(o.value + k)) tags[d + k] = tags[o.value + k];
                const std::string a = S(d), b = S(o.value), c = S(d + 1), e = S(o.value + 1);
                emit(a + " = " + b + "; " + c + " = " + e + ";");
                return;
            }
            const std::string a = S(d), b = src(o), c = S(d + 1);
            emit(a + " = " + b + "; " + c + " = 0u;");
            return;
        }
        if (n == "s_wqm_b64") {
            if (d == 126) return; // whole quad mode: helper lanes, nothing to do per lane
            return write_mask(d, mask_of(i.O("ssrc0")));
        }
        if (ends(n, "saveexec_b64")) {
            const std::string a = mask_of(i.O("ssrc0"));
            const std::string op = n.substr(2, n.find("_saveexec") - 2);
            std::string expr;
            if (op == "and")
                expr = a + " && ex";
            else if (op == "or")
                expr = a + " || ex";
            else if (op == "xor")
                expr = a + " != ex";
            else if (op == "andn2")
                expr = a + " && !ex";
            else if (op == "orn2")
                expr = a + " || !ex";
            else if (op == "nand")
                expr = "!(" + a + " && ex)";
            else if (op == "nor")
                expr = "!(" + a + " || ex)";
            else if (op == "xnor")
                expr = a + " == ex";
            else
                throw Unsupported(n);
            emit("{ bool t_ = " + expr + "; " + M(d) + " = ex; ex = t_; scc = ex; }");
            return;
        }
        if (n == "s_not_b64") return write_mask(d, "!(" + mask_of(i.O("ssrc0")) + ")");
        if (i.fmt == "SOP2") {
            const std::string a = mask_of(i.O("ssrc0")), b = mask_of(i.O("ssrc1"));
            const std::string op = n.substr(2, n.find("_b64") - 2);
            std::string expr;
            if (op == "and")
                expr = "(" + a + " && " + b + ")";
            else if (op == "or")
                expr = "(" + a + " || " + b + ")";
            else if (op == "xor")
                expr = "(" + a + " != " + b + ")";
            else if (op == "andn2")
                expr = "(" + a + " && !" + b + ")";
            else if (op == "orn2")
                expr = "(" + a + " || !" + b + ")";
            else if (op == "nand")
                expr = "!(" + a + " && " + b + ")";
            else if (op == "nor")
                expr = "!(" + a + " || " + b + ")";
            else if (op == "xnor")
                expr = "(" + a + " == " + b + ")";
            else if (op == "cselect")
                expr = "(scc ? " + a + " : " + b + ")";
            else
                throw Unsupported(n);
            if (op != "cselect") {
                const std::string target = d != 106 && d != 126 ? M(d) : (d == 106 ? "vcc" : "ex");
                emit("{ bool t_ = " + expr + "; " + target + " = t_; scc = t_; }");
            } else {
                emit("");
                write_mask(d, expr);
            }
            return;
        }
        throw Unsupported(n);
    }

    // ---- vector ALU
    void valu(const Inst& i, size_t k) {
        const std::string& n = i.name;
        const std::string base = i.has("base") ? i.S("base") : i.fmt;
        if (base == "VOPC" || i.fmt == "VOPC") return vcmp(i);
        const int64_t d = i.I("vdst");
        const std::string a = fsrc(i, 0);
        const std::string ua = src(i.O("src0"));
        std::string b, c, ub, uc;
        if (i.fmt == "VOP3") {
            b = fsrc(i, 1);
            c = fsrc(i, 2);
            ub = src(i.O("src1"));
            uc = src(i.O("src2"));
            if (n == "v_mac_f32" || n == "v_mac_legacy_f32") {
                // VOP3-encoded v_mac: the addend is the destination, src2 is unused
                c = f32(V(d));
                uc = V(d);
            }
        } else if (i.fmt == "VOP2") {
            b = f32(vsrc1(i));
            c = f32(V(d));
            ub = vsrc1(i);
            uc = V(d);
        }
        auto F = [&](const std::string& e) { write_v(i, d, e, true); };
        auto U = [&](const std::string& e) { write_v(i, d, e, false); };
        if (n == "v_add_f32") return F(a + " + " + b);
        if (n == "v_sub_f32") return F(a + " - " + b);
        if (n == "v_subrev_f32") return F(b + " - " + a);
        if (n == "v_mul_f32") return F(a + " * " + b);
        if (n == "v_mul_legacy_f32")
            return F("((" + a + ") == 0.0f || (" + b + ") == 0.0f ? 0.0f : " + a + " * " + b + ")");
        if (n == "v_mac_f32" || n == "v_mac_legacy_f32" || n == "v_mad_f32" || n == "v_mad_legacy_f32" ||
            n == "v_fma_f32")
            return F("mad(" + a + ", " + b + ", " + c + ")");
        if (n == "v_madmk_f32") return F("mad(" + a + ", asfloat(0x" + hex8(i.I("lit")) + "u), " + f32(vsrc1(i)) + ")");
        if (n == "v_madak_f32") return F("mad(" + a + ", " + f32(vsrc1(i)) + ", asfloat(0x" + hex8(i.I("lit")) + "u))");
        if (n == "v_min_f32") return F("min(" + a + ", " + b + ")");
        if (n == "v_max_f32") return F("max(" + a + ", " + b + ")");
        if (n == "v_min_legacy_f32") return F("min(" + a + ", " + b + ")");
        if (n == "v_max_legacy_f32") return F("max(" + a + ", " + b + ")");
        if (n == "v_min3_f32") return F("min(min(" + a + ", " + b + "), " + c + ")");
        if (n == "v_max3_f32") return F("max(max(" + a + ", " + b + "), " + c + ")");
        if (n == "v_med3_f32") return F("max(min(" + a + ", " + b + "), min(max(" + a + ", " + b + "), " + c + "))");
        if (n == "v_ldexp_f32") return F("ldexp(" + a + ", float(int(" + (i.fmt == "VOP3" ? ub : vsrc1(i)) + ")))");
        if (n == "v_mov_b32") return U(ua);
        if (n == "v_not_b32") return U("~" + ua);
        if (n == "v_fract_f32") return F("frac(" + a + ")");
        if (n == "v_trunc_f32") return F("trunc(" + a + ")");
        if (n == "v_ceil_f32") return F("ceil(" + a + ")");
        if (n == "v_floor_f32") return F(floor_expr(i, k, a));
        if (n == "v_rndne_f32") return F("round(" + a + ")");
        if (n == "v_exp_f32" || n == "v_exp_legacy_f32") return F("exp2(" + a + ")");
        if (n == "v_log_f32" || n == "v_log_clamp_f32" || n == "v_log_legacy_f32") return F("log2(" + a + ")");
        if (n == "v_rcp_f32" || n == "v_rcp_iflag_f32" || n == "v_rcp_legacy_f32" || n == "v_rcp_clamp_f32")
            return F("rcp(" + a + ")");
        if (n == "v_rsq_f32") return F("rsqrt(" + a + ")");
        if (n == "v_rsq_clamp_f32") return F("min(rsqrt(" + a + "), 3.402823e38f)");
        if (n == "v_rsq_legacy_f32") return F("rsqrt(" + a + ")");
        if (n == "v_sqrt_f32") return F("sqrt(" + a + ")");
        if (n == "v_sin_f32") return F("sin(" + a + " * 6.28318548f)");
        if (n == "v_cos_f32") return F("cos(" + a + " * 6.28318548f)");
        if (n == "v_cvt_f32_i32") return F("float(int(" + ua + "))");
        if (n == "v_cvt_f32_u32") return F("float(" + ua + ")");
        if (n == "v_cvt_i32_f32") return U("uint(int(" + a + "))");
        if (n == "v_cvt_u32_f32") return U("uint(" + a + ")");
        if (n == "v_cvt_flr_i32_f32") return U("uint(int(floor(" + a + ")))");
        if (n == "v_cvt_rpi_i32_f32") return U("uint(int(floor(" + a + " + 0.5f)))");
        if (n == "v_cvt_f16_f32") return U("f32tof16(" + a + ")");
        if (n == "v_cvt_f32_f16") return F("f16tof32(" + ua + ")");
        if (n == "v_cvt_f32_ubyte0") return F("float(" + ua + " & 0xFFu)");
        if (n == "v_cvt_f32_ubyte1") return F("float((" + ua + " >> 8) & 0xFFu)");
        if (n == "v_cvt_f32_ubyte2") return F("float((" + ua + " >> 16) & 0xFFu)");
        if (n == "v_cvt_f32_ubyte3") return F("float(" + ua + " >> 24)");
        if (n == "v_cvt_off_f32_i4") return F("float(int(" + ua + " << 28) >> 28) * 0.0625f");
        if (n == "v_cvt_pkrtz_f16_f32") return U("(f32tof16(" + a + ") | (f32tof16(" + b + ") << 16))");
        if (n == "v_bfrev_b32") return U("reversebits(" + ua + ")");
        if (n == "v_ffbh_u32")
            return U("(firstbithigh(" + ua + ") == 0xFFFFFFFFu ? 0xFFFFFFFFu : 31u - firstbithigh(" + ua + "))");
        if (n == "v_ffbl_b32") return U("firstbitlow(" + ua + ")");
        if (n == "v_and_b32") return U("(" + ua + " & " + ub + ")");
        if (n == "v_or_b32") return U("(" + ua + " | " + ub + ")");
        if (n == "v_xor_b32") return U("(" + ua + " ^ " + ub + ")");
        if (n == "v_lshl_b32") return U("(" + ua + " << (" + ub + " & 31u))");
        if (n == "v_lshr_b32") return U("(" + ua + " >> (" + ub + " & 31u))");
        if (n == "v_lshlrev_b32") return U("(" + ub + " << (" + ua + " & 31u))");
        if (n == "v_lshrrev_b32") return U("(" + ub + " >> (" + ua + " & 31u))");
        if (n == "v_ashr_i32") return U("uint(int(" + ua + ") >> (" + ub + " & 31u))");
        if (n == "v_ashrrev_i32") return U("uint(int(" + ub + ") >> (" + ua + " & 31u))");
        if (n == "v_bfm_b32") return U("(((1u << (" + ua + " & 31u)) - 1u) << (" + ub + " & 31u))");
        if (n == "v_bcnt_u32_b32") return U("(countbits(" + ua + ") + " + ub + ")");
        if (n == "v_mbcnt_lo_u32_b32" || n == "v_mbcnt_hi_u32_b32") return U(ub);
        if (n == "v_min_u32") return U("min(" + ua + ", " + ub + ")");
        if (n == "v_max_u32") return U("max(" + ua + ", " + ub + ")");
        if (n == "v_min_i32") return U("uint(min(int(" + ua + "), int(" + ub + ")))");
        if (n == "v_max_i32") return U("uint(max(int(" + ua + "), int(" + ub + ")))");
        if (n == "v_mul_u32_u24") return U("((" + ua + " & 0xFFFFFFu) * (" + ub + " & 0xFFFFFFu))");
        if (n == "v_mul_i32_i24") return U("uint((int(" + ua + " << 8) >> 8) * (int(" + ub + " << 8) >> 8))");
        if (n == "v_mad_u32_u24") return U("((" + ua + " & 0xFFFFFFu) * (" + ub + " & 0xFFFFFFu) + " + uc + ")");
        if (n == "v_mad_i32_i24")
            return U("uint((int(" + ua + " << 8) >> 8) * (int(" + ub + " << 8) >> 8) + int(" + uc + "))");
        if (n == "v_mul_lo_u32" || n == "v_mul_lo_i32") return U("(" + ua + " * " + ub + ")");
        if (n == "v_bfe_u32")
            return U("((" + ua + " >> (" + ub + " & 31u)) & ((" + uc + " & 31u) == 0u ? 0u : (0xFFFFFFFFu >> (32u - (" +
                     uc + " & 31u)))))");
        if (n == "v_bfe_i32")
            return U("((" + uc + " & 31u) == 0u ? 0u : uint(int(" + ua + " << (32u - (" + uc + " & 31u) - (" + ub +
                     " & 31u))) >> (32u - (" + uc + " & 31u))))");
        if (n == "v_bfi_b32") return U("((" + ua + " & " + ub + ") | (~" + ua + " & " + uc + "))");
        if (n == "v_alignbit_b32")
            return U("((" + uc + " & 31u) == 0u ? " + ub + " : ((" + ub + " >> (" + uc + " & 31u)) | (" + ua +
                     " << (32u - (" + uc + " & 31u)))))");
        if (n == "v_min3_i32") return U("uint(min(min(int(" + ua + "), int(" + ub + ")), int(" + uc + ")))");
        if (n == "v_max3_i32") return U("uint(max(max(int(" + ua + "), int(" + ub + ")), int(" + uc + ")))");
        if (n == "v_min3_u32") return U("min(min(" + ua + ", " + ub + "), " + uc + ")");
        if (n == "v_max3_u32") return U("max(max(" + ua + ", " + ub + "), " + uc + ")");
        if (n == "v_sad_u32")
            return U("((" + ua + " > " + ub + " ? " + ua + " - " + ub + " : " + ub + " - " + ua + ") + " + uc + ")");
        if (n == "v_cubeid_f32") return F("cubeid(" + a + ", " + b + ", " + c + ")");
        if (n == "v_cubesc_f32") return F("cubesc(" + a + ", " + b + ", " + c + ")");
        if (n == "v_cubetc_f32") return F("cubetc(" + a + ", " + b + ", " + c + ")");
        if (n == "v_cubema_f32") return F("cubema(" + a + ", " + b + ", " + c + ")");
        if (n == "v_cndmask_b32") {
            const std::string m = i.fmt == "VOP2" ? "vcc" : mask_of(i.O("src2"));
            return U("(" + m + " ? " + ub + " : " + ua + ")");
        }
        if (n == "v_add_i32" || n == "v_sub_i32" || n == "v_subrev_i32" || n == "v_addc_u32" || n == "v_subb_u32") {
            const std::string x = n == "v_subrev_i32" ? ub : ua, y = n == "v_subrev_i32" ? ua : ub;
            const std::string op = contains(n, "add") ? "+" : "-";
            std::string cin;
            if (n == "v_addc_u32" || n == "v_subb_u32") {
                const std::string cm = i.fmt == "VOP2" ? "vcc" : mask_of(i.O("src2"));
                cin = " " + op + " (" + cm + " ? 1u : 0u)";
            }
            const int64_t sd = i.fmt == "VOP2" ? 106 : i.I("sdst");
            const std::string res = "(" + x + " " + op + " " + y + cin + ")";
            const std::string carry = op == "+" ? "((" + x + " + " + y + ") < " + x + ")" : "(" + y + " > " + x + ")";
            if (sd != 106 && sd != 126 && !mask_pairs.count(sd)) mask_pairs.insert(sd);
            const std::string vd = V(d);
            const std::string m = sd != 106 ? M(sd) : "vcc";
            emit("{ bool c_ = " + carry + "; " + (exec_written ? "if (ex) " : "") + vd + " = " + res + "; " + m +
                 " = c_; }");
            return;
        }
        if (n == "v_readfirstlane_b32") return write_s(d, ua);
        if (n == "v_readlane_b32") {
            const int64_t lane = i.fmt == "VOP2" ? i.geti("vsrc1").value_or(kNone) : kNone;
            const Operand& s0 = i.O("src0");
            if (s0.kind == Operand::V && lane_spills.count({s0.value, lane}))
                return write_s(d, "lane_v" + num(s0.value) + "_" + lane_str(lane));
            return write_s(d, ua);
        }
        if (n == "v_writelane_b32") {
            const int64_t lane = i.geti("vsrc1").value_or(kNone);
            lane_spills.insert({d, lane});
            return emit("lane_v" + num(d) + "_" + lane_str(lane) + " = " + ua + ";");
        }
        throw Unsupported(n);
    }
    static std::string lane_str(int64_t l) { return l == kNone ? "None" : num(l); }

    void vcmp(const Inst& i) {
        const std::string& n = i.name;
        const auto parts = split(n, '_'); // v_cmp[x]_<op>_<type>
        if (parts.size() < 4) throw Unsupported(n);
        const bool x = ends(parts[1], "x");
        const std::string op = parts[2], ty = parts[3];
        const std::string a = src(i.O("src0"));
        const std::string b = i.fmt == "VOP3" ? src(i.O("src1")) : V(i.I("vsrc1"));
        std::string expr;
        if (ty == "f32") {
            const std::string fa = fsrc(i, 0);
            const std::string fb = i.fmt == "VOP3" ? fsrc(i, 1) : f32(b);
            if (op == "class")
                expr = "cmp_class(" + fa + ", " + b + ")";
            else if (op == "f")
                expr = "false";
            else if (op == "lt")
                expr = fa + " < " + fb;
            else if (op == "eq")
                expr = fa + " == " + fb;
            else if (op == "le")
                expr = fa + " <= " + fb;
            else if (op == "gt")
                expr = fa + " > " + fb;
            else if (op == "lg")
                expr = fa + " != " + fb;
            else if (op == "ge")
                expr = fa + " >= " + fb;
            else if (op == "o")
                expr = "!(isnan(" + fa + ") || isnan(" + fb + "))";
            else if (op == "u")
                expr = "(isnan(" + fa + ") || isnan(" + fb + "))";
            else if (op == "nge")
                expr = "!(" + fa + " >= " + fb + ")";
            else if (op == "nlg")
                expr = "!(" + fa + " != " + fb + ")";
            else if (op == "ngt")
                expr = "!(" + fa + " > " + fb + ")";
            else if (op == "nle")
                expr = "!(" + fa + " <= " + fb + ")";
            else if (op == "neq")
                expr = "!(" + fa + " == " + fb + ")";
            else if (op == "nlt")
                expr = "!(" + fa + " < " + fb + ")";
            else if (op == "tru")
                expr = "true";
            else
                throw Unsupported(n);
        } else if (ty == "i32" || ty == "u32") {
            const std::string ca = ty == "i32" ? "int(" + a + ")" : a, cb = ty == "i32" ? "int(" + b + ")" : b;
            if (op == "f")
                expr = "false";
            else if (op == "lt")
                expr = ca + " < " + cb;
            else if (op == "eq")
                expr = ca + " == " + cb;
            else if (op == "le")
                expr = ca + " <= " + cb;
            else if (op == "gt")
                expr = ca + " > " + cb;
            else if (op == "ne")
                expr = ca + " != " + cb;
            else if (op == "ge")
                expr = ca + " >= " + cb;
            else if (op == "t")
                expr = "true";
            else
                throw Unsupported(n);
        } else {
            throw Unsupported(n);
        }
        const int64_t sd = i.fmt == "VOPC" ? 106 : i.I("sdst");
        const std::string guard = exec_written ? "ex && " : "";
        if (x) {
            const std::string m = sd == 106 ? "vcc" : M(sd);
            emit("{ bool c_ = " + guard + "(" + expr + "); ex = c_; " + m + " = c_; }");
        } else {
            const std::string m = sd == 106 ? "vcc" : M(sd);
            emit(m + " = " + guard + "(" + expr + ");");
        }
    }

    // ---- memory: scalar loads (descriptors / constants)
    void smrd(const Inst& i) {
        const std::string& n = i.name;
        const int64_t base = i.I("sbase"), d = i.I("sdst");
        int cnt = 1;
        for (auto [k, c] : {std::pair{"x2", 2}, {"x4", 4}, {"x8", 8}, {"x16", 16}})
            if (ends(n, k)) cnt = c;
        const Tag* t = tag(base);
        if (starts(n, "s_buffer_load")) {
            if (!t || (t->kind != "CB" && t->kind != "B" && t->kind != "V"))
                throw Unsupported("s_buffer_load from untracked");
            const int64_t slot = t->a;
            cbuffers.insert(slot);
            if (i.I("imm") || i.has("lit")) {
                // imm offsets and CI 32-bit literal offsets are in dwords
                const int64_t off = i.I("imm") ? i.I("offset") : i.I("lit");
                for (int k = 0; k < cnt; k++) {
                    const int64_t dw = off + k;
                    write_s(d + k, "cb" + num(slot) + "[" + num(dw >> 2) + "]." + comp(dw & 3));
                }
            } else {
                // SGPR offsets are in bytes
                const std::string o = S(i.I("offset"));
                for (int k = 0; k < cnt; k++)
                    write_s(d + k, "cbload" + num(slot) + "((" + o + " >> 2) + " + num(k) + "u)");
            }
            return;
        }
        if (!t || t->kind != "P") throw Unsupported("s_load from untracked base");
        const int64_t ptr = t->a;
        if (!i.I("imm")) throw Unsupported("s_load with register offset");
        const int64_t off = i.I("offset");
        if (ptr == U_EUD) {
            // resolve which usage slot lives at extended user data dword `off`
            bool found = false;
            for (const auto& s : meta.slots)
                if (s.reg == 16 + off) {
                    tag_slot(s.usage, s.api, d, s.flags);
                    found = true;
                    break;
                }
            if (!found) throw Unsupported("extended user data offset " + num(off));
        } else if (ptr == U_RT) {
            for (int k = 0; k < cnt; k++) tags[d + k] = {"T", off / 8, k};
        } else if (ptr == U_IGT) {
            for (int k = 0; k < cnt; k++) tags[d + k] = {"RING", (off + k) / 4, (off + k) % 4};
        } else if (ptr == U_ST) {
            for (int k = 0; k < cnt; k++) tags[d + k] = {"S", off / 4, k};
        } else if (ptr == U_CBT) {
            for (int k = 0; k < cnt; k++) tags[d + k] = {"CB", off / 4, k};
        } else if (ptr == U_VBT) {
            for (int k = 0; k < cnt; k++) tags[d + k] = {"V", off / 4, (off + k) % 4};
        } else {
            throw Unsupported("s_load from pointer usage");
        }
        for (int k = 0; k < cnt; k++) sregs.insert(d + k);
    }

    // ES->GS / GS->VS ring traffic of a geometry pipeline, mapped onto D3D11 GS I/O.
    void ring_access(const Inst& i) {
        const std::string& n = i.name;
        if (n != "buffer_load_dword" && n != "buffer_store_dword") throw Unsupported(n + " on a ring");
        const std::string& role = ring.role;
        const int64_t imm = i.I("offset");
        const std::string va = i.I("offen") ? V(i.I("vaddr")) : "0u";
        const Operand& so = i.O("soffset");
        const std::string soff = (so.kind == Operand::Int && so.value == 0) ? "0u" : src(so);
        const std::string guard = exec_written ? "if (ex) " : "";
        const int64_t mv = ring.max_vert;
        if (n == "buffer_store_dword") {
            const std::string v = V(i.I("vdata"));
            if (role == "es") {
                const int64_t k = imm >> 2;
                if (i.I("offen") || k >= 4 * ES_VECS) throw Unsupported("ES ring store layout");
                return emit(guard + "o.e[" + num(k >> 2) + "]." + comp(k & 3) + " = asfloat(" + v + ");");
            }
            if (role == "gs")
                return emit(guard + "gso[min(((" + va + " + " + num(imm) + "u) >> 2) / " + num(mv) + "u, " +
                            num(GSO_LEN - 1) + "u)] = " + v + ";");
            throw Unsupported("ring store outside ES/GS");
        }
        const std::string dd = V(i.I("vdata"));
        if (role == "gs")
            return emit("{ uint vi_ = min(" + va + " >> 2, 2u), k_ = (" + soff + " + " + num(imm) + "u) >> 8; " +
                        guard + dd + " = asuint(IN[vi_].e[k_ >> 2][k_ & 3u]); }");
        if (role == "copy")
            return emit(guard + dd + " = gso[min((" + va + " + " + soff + " + " + num(imm) + "u) / " + num(64 * mv) +
                        "u, " + num(GSO_LEN - 1) + "u)];");
        throw Unsupported("ring load outside GS/copy shader");
    }

    void op_s_sendmsg(const Inst& i) {
        const int64_t m = i.I("simm16");
        const int64_t msg = m & 0xF, op = (m >> 4) & 3;
        if (msg != 2 || kind != "gs") return; // GS_DONE, interrupts, ...
        const std::string guard = exec_written ? "if (ex) " : "";
        if (op == 2 || op == 3) emit(guard + "{ gsv = copyvs(gso); stream.Append(gsv); }");
        if (op == 1 || op == 3) emit(guard + "stream.RestartStrip();");
    }

    // ---- buffer loads (constant-buffer backed: e.g. bone palettes indexed per vertex)
    void buffer_load(const Inst& i) {
        const std::string& n = i.name;
        if (!contains(n, "load")) throw Unsupported(n);
        const Tag* t = tag(i.I("srsrc"));
        if (!t || t->kind != "CB") throw Unsupported(n + " from untracked descriptor");
        int count = 0;
        auto suffix = [&](const std::string& s) {
            const std::string last = s.substr(s.rfind('_') + 1);
            if (last == "x") return 1;
            if (last == "xy") return 2;
            if (last == "xyz") return 3;
            if (last == "xyzw") return 4;
            throw Unsupported(s);
        };
        if (i.fmt == "MTBUF") {
            const int64_t df = i.I("dfmt"), nf = i.I("nfmt");
            if (!(df == 4 || df == 11 || df == 13 || df == 14) || !(nf == 4 || nf == 5 || nf == 7))
                throw Unsupported("tbuffer format");
            count = suffix(n);
        } else if (starts(n, "buffer_load_dword")) {
            if (n == "buffer_load_dword")
                count = 1;
            else if (n == "buffer_load_dwordx2")
                count = 2;
            else if (n == "buffer_load_dwordx3")
                count = 3;
            else if (n == "buffer_load_dwordx4")
                count = 4;
            else
                throw Unsupported(n);
        } else if (starts(n, "buffer_load_format")) {
            count = suffix(n);
        } else {
            throw Unsupported(n);
        }
        const int64_t slot = t->a;
        cbuffers.insert(slot);
        std::vector<std::string> parts = {num(i.I("offset")) + "u"};
        int64_t k = 0;
        if (i.I("idxen")) {
            parts.push_back(V(i.I("vaddr")) + " * 16u"); // constant buffer records are 16 bytes
            k = 1;
        }
        if (i.I("offen")) parts.push_back(V(i.I("vaddr") + k));
        const Operand& so = i.O("soffset");
        if (!(so.kind == Operand::Int && so.value == 0)) parts.push_back(src(so));
        emit("{ uint b_ = (" + join(parts, " + ") + ") >> 2;");
        for (int c = 0; c < count; c++)
            emit(std::string("    ") + (exec_written ? "if (ex) " : "") + V(i.I("vdata") + c) + " = cbload" +
                 num(slot) + "(b_ + " + num(c) + "u);");
        emit("}");
    }

    // ---- images
    void mimg(const Inst& i) {
        const std::string& n = i.name;
        const Tag* rt = tag(i.I("srsrc"));
        if (!rt || rt->kind != "T") throw Unsupported(n + " with untracked resource");
        const int64_t slot = rt->a;
        const int64_t a = i.I("vaddr");
        auto av = [&](int64_t k) { return f32(V(a + k)); };
        int dims = i.I("da") ? 3 : 2;
        std::vector<int> comps;
        for (int k = 0; k < 4; k++)
            if ((i.I("dmask") >> k) & 1) comps.push_back(k);
        const int64_t dst = i.I("vdata");
        auto store = [&](std::string expr, bool swizzle = true) {
            if (swizzle) {
                // channel order of the bound view (RT component swap + T# dst_sel), supplied by the runtime
                expr = "ctr_swizzle(" + expr + ", ctr_swz[" + num(slot >> 2) + "][" + num(slot & 3) + "])";
                uses_swizzle = true;
            }
            emit("{ float4 r_ = " + expr + ";");
            for (size_t j = 0; j < comps.size(); j++)
                emit(std::string("    ") + (exec_written ? "if (ex) " : "") + V(dst + (int64_t)j) + " = asuint(r_." +
                     comp(comps[j]) + ");");
            emit("}");
        };
        if (n == "image_load_mip" || n == "image_load") {
            const std::string mip = n == "image_load_mip" ? "int(" + V(a + dims) + ")" : "0";
            const std::string ts = "ctr_tscale[" + num(slot >> 2) + "][" + num(slot & 3) + "]";
            uses_scale = true;
            std::vector<std::string> cs;
            for (int k = 0; k < dims; k++)
                cs.push_back(k < 2 ? "int(float(int(" + V(a + k) + ")) * " + ts + ")" : "int(" + V(a + k) + ")");
            set_texture(slot, dims, "load");
            return store("t" + num(slot) + ".Load(int" + num(dims + 1) + "(" + join(cs, ", ") + ", " + mip + "))");
        }
        if (n == "image_get_lod") {
            const Tag* ss = tag(i.I("ssamp"));
            if (!ss || ss->kind != "S") throw Unsupported("get_lod sampler");
            samplers.emplace(ss->a, "normal");
            set_texture(slot, dims, "sample");
            std::vector<std::string> cs;
            for (int k = 0; k < dims; k++) cs.push_back(av(k));
            const std::string c = "float" + num(dims) + "(" + join(cs, ", ") + ")";
            const std::string sm = "sm" + num(ss->a);
            return store("float4(t" + num(slot) + ".CalculateLevelOfDetail(" + sm + ", " + c + "), t" + num(slot) +
                             ".CalculateLevelOfDetailUnclamped(" + sm + ", " + c + "), 0, 0)",
                         false);
        }
        if (n == "image_get_resinfo") {
            uses_scale = true;
            set_texture(slot, dims, "load");
            return store("texinfo" + num(slot) + "(int(" + V(a) + "))", false);
        }
        if (!(starts(n, "image_sample") || starts(n, "image_gather4"))) throw Unsupported(n);
        std::optional<Tag> ss;
        if (const Tag* t = tag(i.I("ssamp")); t && t->kind == "S")
            ss = *t;
        else
            ss = loop_carried_sampler(i.I("ssamp"));
        if (!ss || ss->kind != "S") throw Unsupported(n + " with untracked sampler");
        const int64_t smp = ss->a;
        std::string mods = starts(n, "image_sample") ? n.substr(n.rfind("image_sample") + 12)
                                                     : n.substr(n.rfind("image_gather4") + 13);
        int64_t k = 0;
        std::optional<std::string> offset;
        if (ends(mods, "_o")) {
            offset = V(a + k);
            k += 1;
            mods = mods.substr(0, mods.size() - 2);
        }
        std::optional<std::string> bias;
        if (contains(mods, "_b")) {
            bias = av(k);
            k += 1;
        }
        std::optional<std::string> cmp_;
        if (starts(mods, "_c")) {
            cmp_ = av(k);
            k += 1;
        }
        std::vector<std::string> grads;
        if (contains(mods, "_d") || contains(mods, "_cd")) {
            for (int j = 0; j < 4; j++) grads.push_back(av(k + j));
            k += 4;
        }
        std::string coord;
        bool cube = false;
        if (cube_face_vgprs.count(a + k + 2)) {
            // GCN cube lookup: (s, t) in [1, 2] + face index. ctr_cube<slot> turns it back into a
            // direction when the slot is declared TextureCube (assemble), else keeps the 6-slice array
            // coordinate; explicit gradients and offsets are face-space, so they keep the array
            dims = 3;
            coord = "float3(" + av(k) + " - 1.0f, " + av(k + 1) + " - 1.0f, " + av(k + 2) + ")";
            cube = grads.empty() && !offset;
            if (cube) coord = "ctr_cube" + num(slot) + "(" + coord + ")";
        } else {
            std::vector<std::string> cs;
            for (int j = 0; j < dims; j++) cs.push_back(av(k + j));
            coord = "float" + num(dims) + "(" + join(cs, ", ") + ")";
        }
        dims_seen[slot].insert(dims);
        auto fd = force_dims.find(slot);
        if ((fd != force_dims.end() ? fd->second : dims) == 3 && dims == 2) {
            coord = "float3(" + coord + ", 0.0f)";
            k += 2;
            dims = 3;
        } else {
            k += dims;
        }
        std::optional<std::string> lod;
        if (contains(mods, "_l") && !contains(mods, "_lz")) lod = av(k);
        if (contains(mods, "_lz")) lod = "0.0f";
        set_texture(slot, dims, "sample", cube);
        const std::string tex = "t" + num(slot);
        if (cmp_) {
            auto it = samplers.find(smp);
            if (it != samplers.end() && it->second == "normal")
                throw Unsupported("sampler used as both compare and normal");
            samplers[smp] = "cmp";
        } else {
            auto it = samplers.find(smp);
            if (it != samplers.end() && it->second == "cmp")
                throw Unsupported("sampler used as both compare and normal");
            samplers[smp] = "normal";
        }
        const std::string sm = "sm" + num(smp);
        if (offset) coord = "(" + coord + " + texoff" + num(dims) + "(" + tex + ", " + *offset + "))";
        if (starts(n, "image_gather4")) {
            static const char* chans[] = {"Red", "Green", "Blue", "Alpha"};
            const std::string ch = comps.empty() ? "Red" : chans[comps[0]];
            comps = {0, 1, 2, 3};
            if (cmp_) return store(tex + ".GatherCmp" + ch + "(" + sm + ", " + coord + ", " + *cmp_ + ")", false);
            return store(tex + ".Gather" + ch + "(" + sm + ", " + coord + ")", false);
        }
        if (cmp_) {
            if (lod && *lod != "0.0f") throw Unsupported("compare with explicit lod");
            const std::string fn = (lod || kind != "ps") ? "SampleCmpLevelZero" : "SampleCmp";
            return store(tex + "." + fn + "(" + sm + ", " + coord + ", " + *cmp_ + ").xxxx", false);
        }
        if (!grads.empty()) {
            const std::string z = dims == 3 ? ", 0" : "";
            const std::string gx = "float" + num(dims) + "(" + grads[0] + ", " + grads[1] + z + ")";
            const std::string gy = "float" + num(dims) + "(" + grads[2] + ", " + grads[3] + z + ")";
            return store(tex + ".SampleGrad(" + sm + ", " + coord + ", " + gx + ", " + gy + ")");
        }
        if (lod) return store(tex + ".SampleLevel(" + sm + ", " + coord + ", " + *lod + ")");
        if (kind != "ps") return store(tex + ".SampleLevel(" + sm + ", " + coord + ", 0)");
        if (bias) return store(tex + ".SampleBias(" + sm + ", " + coord + ", " + *bias + ")");
        return store(tex + ".Sample(" + sm + ", " + coord + ")");
    }

    // ---- interpolation / exports
    void vintrp(const Inst& i, size_t k) {
        const int64_t attr = i.I("attr");
        const auto& ins = meta.inputs;
        if (attr >= (int64_t)ins.size()) throw Unsupported("interp attr out of range");
        const int sem = ins[attr].sem;
        const std::string mode =
            (i.name == "v_interp_mov_f32" || ins[attr].flat) ? "flat" : (ins[attr].linear ? "linear" : "persp");
        inputs_used[sem] = mode;
        if (i.name == "v_interp_p1_f32") return;
        write_v(i, i.I("vdst"), "IN.a" + num(sem) + "." + comp(i.I("chan")), true);
    }
    void exp(const Inst& i) {
        const int64_t tgt = i.I("tgt"), en = i.I("en");
        const auto& src_ = i.find("vsrc")->list;
        if (tgt == 9) return; // null export
        if (kind == "es" || kind == "gs") throw Unsupported(kind + " export");
        if (kind == "vs") {
            if (tgt == 12) {
                std::vector<std::string> vals;
                for (int k = 0; k < 4; k++)
                    vals.push_back((en >> k) & 1 ? f32(V(src_[k])) : (k == 3 ? "1.0f" : "0.0f"));
                emit("o.pos = float4(" + join(vals, ", ") + ");");
                return;
            }
            if (tgt >= 13 && tgt <= 15) return; // misc / clip distances
            if (tgt >= 32 && tgt < 64) {
                const int64_t p = tgt - 32;
                std::optional<int> sem;
                for (const auto& e : meta.exports)
                    if (e.out == p) {
                        sem = e.sem;
                        break;
                    }
                if (!sem) return;
                outputs.insert(*sem);
                std::vector<std::string> vals;
                for (int k = 0; k < 4; k++) vals.push_back((en >> k) & 1 ? f32(V(src_[k])) : "0.0f");
                emit("o.a" + num(*sem) + " = float4(" + join(vals, ", ") + ");");
                return;
            }
            throw Unsupported("vs export target " + num(tgt));
        }
        // pixel shader
        if (tgt == 8) {
            if (en & 1) {
                depth_out = true;
                emit("o.depth = " + f32(V(src_[0])) + ";");
            }
            return;
        }
        if (tgt < 8) {
            outputs.insert(tgt);
            if (i.I("compr")) {
                const std::string v0 = V(src_[0]), v1 = V(src_[1]);
                emit("o.c" + num(tgt) + " = float4(f16tof32(" + v0 + " & 0xFFFFu), f16tof32(" + v0 +
                     " >> 16), f16tof32(" + v1 + " & 0xFFFFu), f16tof32(" + v1 + " >> 16));");
            } else {
                std::vector<std::string> vals;
                for (int k = 0; k < 4; k++) vals.push_back((en >> k) & 1 ? f32(V(src_[k])) : "0.0f");
                emit("o.c" + num(tgt) + " = float4(" + join(vals, ", ") + ");");
            }
            if (i.I("done")) emit("if (!ex) discard;");
            return;
        }
        throw Unsupported("ps export target " + num(tgt));
    }
    // an S# loaded later in program order (inside a loop, used on the next iteration)
    std::optional<Tag> loop_carried_sampler(int64_t reg) {
        for (const auto& i : insts) {
            if (i.name == "s_load_dwordx4" && i.I("sdst") == reg && i.I("imm")) {
                const Tag* t = tag(i.I("sbase"));
                if (t && t->kind == "P" && t->a == U_ST) return Tag{"S", i.I("offset") / 4, 0};
            }
        }
        return std::nullopt;
    }
    // LDS addressed by lane id: per-lane private storage (register spills)
    void ds_private(const Inst& i) {
        if (i.I("gds") || !lane_addr_vgprs.count(i.I("addr"))) throw Unsupported(i.name + " with shared address");
        const int64_t off = i.I("offset0") | (i.I("offset1") << 8);
        lds_slots.insert(off);
        const std::string guard = exec_written ? "if (ex) " : "";
        if (i.name == "ds_write_b32") return emit(guard + "lds" + num(off) + " = " + V(i.I("data0")) + ";");
        emit(guard + V(i.I("vdst")) + " = lds" + num(off) + ";");
    }
    void ds_swizzle(const Inst& i, size_t k) {
        if (kind != "ps") throw Unsupported("ds_swizzle outside pixel shader");
        const int64_t off = i.I("offset0") | (i.I("offset1") << 8);
        if (!(off & 0x8000)) throw Unsupported("ds_swizzle bitmask mode");
        const int64_t perm = off & 0xFF;
        uses_quad = true;
        write_v(i, i.I("vdst"), "quad_swizzle(" + f32(V(i.I("addr"))) + ", " + num(perm) + "u, qlane)", true);
    }
    void op_s_swappc_b64(const Inst& i) {
        if (kind != "vs" && kind != "es") throw Unsupported("s_swappc outside vertex shader");
        uses_vin = true;
        for (const auto& inp : meta.inputs)
            for (int c = 0; c < inp.size; c++)
                emit(V(inp.vgpr + c) + " = ctr_in(IN.a" + num(inp.sem) + "." + comp(c) + ", " + num(inp.sem) + "u);");
    }

    // ------------------------------------------------------------ program
    std::string translate() {
        const size_t body_start = lines.size();
        emit_range(0, insts.size());
        std::vector<std::string> body(lines.begin() + body_start, lines.end());
        lines.resize(body_start);
        return assemble(body);
    }

    std::string assemble(const std::vector<std::string>& body) {
        std::vector<std::string> pin;
        if (kind == "ps") pin = ps_input_vgprs(); // sets uses_scale before the decls
        std::vector<std::string> out = {"// translated from PS4 GCN by tools/gcn2hlsl.py", COMMON_HLSL};
        for (int64_t slot : cbuffers) {
            out.push_back("cbuffer CB" + num(slot) + " : register(b" + num(slot) + ") { uint4 cb" + num(slot) +
                          "[4096]; };");
            out.push_back("uint cbload" + num(slot) + "(uint dw) { return cb" + num(slot) + "[dw >> 2][dw & 3u]; }");
        }
        for (auto& [slot, du] : textures)
            if (cube_slots.count(slot) && !plain_slots.count(slot)) du.first = 4;
        auto sorted_textures = textures;
        std::sort(sorted_textures.begin(), sorted_textures.end(), [](auto& x, auto& y) { return x.first < y.first; });
        for (const auto& [slot, du] : sorted_textures)
            out.push_back(std::string(du.first == 2   ? "Texture2D"
                                      : du.first == 4 ? "TextureCube"
                                                      : "Texture2DArray") +
                          "<float4> t" + num(slot) + " : register(t" + num(slot) + ");");
        if (!cube_slots.empty())
            // (s, t) in [0, 1] and face index (v_cubeid order +X -X +Y -Y +Z -Z) back to a direction,
            // inverting cubesc / cubetc above
            out.push_back("float3 ctr_cubedir(float3 c) { float sc = c.x * 2.0f - 1.0f, tc = c.y * 2.0f - 1.0f; "
                          "uint f = (uint)(c.z + 0.5f); return f == 0u ? float3(1, -tc, -sc) : f == 1u ? float3(-1, -tc, sc) "
                          ": f == 2u ? float3(sc, 1, tc) : f == 3u ? float3(sc, -1, -tc) : f == 4u ? float3(sc, -tc, 1) "
                          ": float3(-sc, -tc, -1); }");
        for (int64_t slot : cube_slots)
            out.push_back("float3 ctr_cube" + num(slot) + "(float3 c) { return " +
                          (plain_slots.count(slot) ? "c" : "ctr_cubedir(c)") + "; }");
        if (uses_swizzle || uses_vin || uses_scale)
            out.push_back("cbuffer CTRSwizzle : register(b13) { uint4 ctr_swz[8]; uint4 ctr_vin[8]; "
                          "float4 ctr_scale; float4 ctr_tscale[8]; };");
        if (uses_vin)
            out.push_back("uint ctr_in(float x, uint sem) { uint m = ctr_vin[sem >> 2][sem & 3u]; uint u = asuint(x); "
                          "return m == 1u ? asuint(float(u)) : m == 2u ? asuint(float(int(u))) : u; }");
        if (uses_swizzle) {
            out.push_back("float ctr_sel(float4 v, uint c) { return c == 0u ? 0.0f : c == 1u ? 1.0f : v[c - 2u]; }");
            out.push_back("float4 ctr_swizzle(float4 v, uint s) { return s == 0u ? v : float4(ctr_sel(v, s & 7u), "
                          "ctr_sel(v, (s >> 3) & 7u), ctr_sel(v, (s >> 6) & 7u), ctr_sel(v, (s >> 9) & 7u)); }");
        }
        for (const auto& [slot, k] : samplers)
            out.push_back(std::string(k == "cmp" ? "SamplerComparisonState" : "SamplerState") + " sm" + num(slot) +
                          " : register(s" + num(slot) + ");");
        bool texinfo = false;
        for (const auto& l : body)
            for (const auto& t : textures)
                if (contains(l, ("texinfo" + num(t.first) + "(").c_str())) texinfo = true;
        if (texinfo) {
            for (const auto& [slot, du] : textures) {
                std::string s = "float4 texinfo" + num(slot) + "(int mip) { uint w, h, d, l; ";
                s += du.first == 2 ? "t" + num(slot) + ".GetDimensions(mip, w, h, l); d = 1;"
                                   : "t" + num(slot) + ".GetDimensions(mip, w, h, d, l);";
                s += " float s_ = ctr_tscale[" + num(slot >> 2) + "][" + num(slot & 3) +
                     "]; w = uint(w / s_ + 0.5f); "
                     "h = uint(h / s_ + 0.5f); return asfloat(uint4(w, h, d, l)); }";
                out.push_back(s);
            }
        }
        for (int dd : {2, 3}) {
            const std::string arr = dd == 3 ? "Array" : "", dz = dd == 3 ? ", d" : "", z = dd == 3 ? ", 0" : "";
            out.push_back("float" + num(dd) + " texoff" + num(dd) + "(Texture2D" + arr +
                          "<float4> t, uint o) { uint w, h" + dz + ", l; t.GetDimensions(0, w, h" + dz +
                          ", l); int2 off = int2(int(o << 26) >> 26, int(o << 18) >> 26); "
                          "return float" +
                          num(dd) + "(off.x / float(w), off.y / float(h)" + z + "); }");
        }
        // I/O structs
        if (kind == "vs" || kind == "es") {
            out.push_back("struct VSIn {");
            for (const auto& inp : meta.inputs)
                out.push_back("    float4 a" + num(inp.sem) + " : ATTRIB" + num(inp.sem) + ";");
            out.push_back("    uint vid : SV_VertexID;\n    uint iid : SV_InstanceID;\n};");
            if (kind == "vs") {
                out.push_back(vsout_struct());
                out.push_back("VSOut main(VSIn IN) {\n    VSOut o = (VSOut)0;");
            } else {
                out.push_back(esout_struct());
                out.push_back("ESOut main(VSIn IN) {\n    ESOut o = (ESOut)0;");
            }
        } else if (kind == "gs") {
            out.push_back(esout_struct());
            out.push_back(vsout_struct());
            out.push_back(ring.copy_fn);
            out.push_back("[maxvertexcount(" + num(ring.out_verts) + ")]");
            out.push_back(
                "void main(triangle ESOut IN[3], uint pid : SV_PrimitiveID, inout TriangleStream<VSOut> stream) {");
            std::vector<std::string> zeros(GSO_LEN, "0u");
            out.push_back("    uint gso[" + num(GSO_LEN) + "] = { " + join(zeros, ", ") + " };");
            out.push_back(
                "    VSOut gsv = (VSOut)0;"); // fxc wants the emitted struct initialised on every path (X4580)
        } else {
            out.push_back("struct PSIn {\n    float4 pos : SV_Position;");
            for (int s = VARYING_FIRST; s < VARYING_END; s++) {
                auto it = inputs_used.find(s);
                const std::string mode = it == inputs_used.end() ? "persp" : it->second;
                const std::string q = mode == "flat" ? "nointerpolation " : mode == "linear" ? "noperspective " : "";
                out.push_back("    " + q + "float4 a" + num(s) + " : VARYING" + num(s) + ";");
            }
            out.push_back("    bool ff : SV_IsFrontFace;\n};");
            out.push_back("struct PSOut {");
            for (int64_t t : outputs) out.push_back("    float4 c" + num(t) + " : SV_Target" + num(t) + ";");
            if (depth_out) out.push_back("    float depth : SV_Depth;");
            if (outputs.empty() && !depth_out) out.push_back("    float4 c0 : SV_Target0;");
            out.push_back("};");
            out.push_back("PSOut main(PSIn IN) {\n    PSOut o = (PSOut)0;");
        }
        for (auto& d : decls()) out.push_back(d);
        if (kind == "vs" || kind == "es") {
            out.push_back(vregs.count(0) ? "    v0 = IN.vid;" : "");
            out.push_back(vregs.count(3) && ((meta.regs.at("rsrc1") >> 24) & 3) == 3 ? "    v3 = IN.iid;" : "");
        } else if (kind == "gs") {
            // vertex offsets into the ES->GS ring stand for the input vertex index
            for (auto [r, val] : {std::pair{0, "0u"}, {1, "1u"}, {2, "pid"}, {3, "2u"}})
                if (vregs.count(r)) out.push_back("    v" + num(r) + " = " + val + ";");
        } else {
            for (auto& p : pin) out.push_back(p);
            if (uses_quad) out.push_back("    uint qlane = (uint(IN.pos.x) & 1u) | ((uint(IN.pos.y) & 1u) << 1);");
        }
        for (auto& l : body) out.push_back(l);
        out.push_back(kind != "gs" ? "    return o;\n}" : "}");
        std::vector<std::string> kept;
        for (auto& x : out)
            if (!x.empty()) kept.push_back(x);
        return join(kept, "\n");
    }

    // the translated program as a helper function (GS copy shader)
    std::string function_text(const std::string& header) {
        const size_t body_start = lines.size();
        emit_range(0, insts.size());
        std::vector<std::string> body(lines.begin() + body_start, lines.end());
        lines.resize(body_start);
        std::vector<std::string> out = {header + " {", "    VSOut o = (VSOut)0;"};
        for (auto& d : decls()) out.push_back(d);
        out.push_back(vregs.count(0) ? "    v0 = 0u;" : "");
        for (auto& l : body) out.push_back(l);
        out.push_back("    return o;\n}");
        std::vector<std::string> kept;
        for (auto& x : out)
            if (!x.empty()) kept.push_back(x);
        return join(kept, "\n");
    }

    std::vector<std::string> decls() {
        std::vector<std::string> decl;
        if (!sregs.empty()) {
            std::vector<std::string> v;
            for (auto n : sregs) v.push_back("s" + num(n) + " = 0");
            decl.push_back("    uint " + join(v, ", ") + ";");
        }
        if (!vregs.empty()) {
            std::vector<std::string> v;
            for (auto n : vregs) v.push_back("v" + num(n) + " = 0");
            decl.push_back("    uint " + join(v, ", ") + ";");
        }
        decl.push_back("    uint m0 = 0, vccl = 0, vcch = 0; bool ex = true, vcc = false, scc = false;");
        if (!masks.empty()) {
            std::vector<std::string> v;
            for (auto n : masks) v.push_back("k" + num(n) + " = false");
            decl.push_back("    bool " + join(v, ", ") + ";");
        }
        if (!lane_spills.empty()) {
            std::vector<std::string> v;
            for (auto [r, l] : lane_spills) v.push_back("lane_v" + num(r) + "_" + lane_str(l) + " = 0");
            decl.push_back("    uint " + join(v, ", ") + ";");
        }
        if (!lds_slots.empty()) {
            std::vector<std::string> v;
            for (auto o : lds_slots) v.push_back("lds" + num(o) + " = 0");
            decl.push_back("    uint " + join(v, ", ") + ";");
        }
        return decl;
    }

    // VGPRs that receive the pixel position x / y (input_addr bits 8 / 9)
    std::map<int64_t, std::string> pos_input_vgprs() const {
        std::map<int64_t, std::string> out;
        if (kind != "ps") return out;
        const uint32_t addr = meta.regs.at("input_addr");
        static const int sizes[] = {2, 2, 2, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int64_t v = 0;
        for (int bit = 0; bit < 16; bit++) {
            if ((addr >> bit) & 1) {
                if (bit == 8 || bit == 9) out[v] = std::string(1, "xy"[bit - 8]);
                v += sizes[bit];
            }
        }
        return out;
    }
    // floor() of the pixel position keeps the sub-pixel position of supersampled targets
    std::string floor_expr(const Inst& i, size_t k, const std::string& a) {
        const Operand* s = i.opt("src0");
        if (s && s->kind == Operand::V) {
            const auto pos = pos_input_vgprs();
            auto it = pos.find(s->value);
            if (it != pos.end()) {
                const std::string needle = "v" + num(s->value) + " = ";
                bool assigned = false;
                for (const auto& line : lines)
                    if (word_match(line, needle)) assigned = true;
                if (!assigned && feeds_float_math_only(i, k, i.geti("vdst"))) {
                    uses_scale = true;
                    return "((floor(IN.pos." + it->second + ") + 0.5f) * ctr_scale.x - 0.5f)";
                }
            }
        }
        return "floor(" + a + ")";
    }
    // True if VGPR `d`, written by insts[k], is only read by float arithmetic before it is overwritten
    bool feeds_float_math_only(const Inst& i, size_t k, std::optional<int64_t> d) const {
        if (!d) return false;
        static const std::set<std::string> float_math = {
            "v_add_f32",        "v_sub_f32", "v_subrev_f32",     "v_mul_f32", "v_mul_legacy_f32", "v_mac_f32",
            "v_mac_legacy_f32", "v_mad_f32", "v_mad_legacy_f32", "v_fma_f32", "v_madmk_f32",      "v_madak_f32"};
        for (size_t jx = k + 1; jx < insts.size() && jx < k + 200; jx++) {
            const Inst& j = insts[jx];
            std::set<int64_t> reads;
            for (const char* key : {"src0", "src1", "src2"})
                if (const Operand* o = j.opt(key); o && o->kind == Operand::V) reads.insert(o->value);
            if (auto v1 = j.geti("vsrc1")) reads.insert(*v1);
            if (j.fmt == "MIMG" || j.fmt == "MUBUF" || j.fmt == "MTBUF" || j.fmt == "DS" || j.fmt == "EXP" ||
                j.fmt == "VINTRP") {
                std::set<std::string> ignore = {"op", "offset", "offset0", "offset1", "dmask", "en", "gds", "vdst"};
                if (starts(j.name, "ds_read") || starts(j.name, "ds_swizzle")) ignore.insert({"data0", "data1"});
                for (const auto& fld : j.f)
                    if (fld.type == Field::Int && !ignore.count(fld.key) && fld.i <= *d && *d < fld.i + 4) return false;
            }
            if (reads.count(*d) && !float_math.count(j.name)) return false;
            if (j.geti("vdst") == d) return true;
            if (j.name == "s_endpgm") return true;
        }
        return true;
    }
    std::vector<std::string> ps_input_vgprs() {
        const uint32_t addr = meta.regs.at("input_addr");
        static const int sizes[] = {2, 2, 2, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        int64_t v = 0;
        std::vector<std::string> out;
        for (int bit = 0; bit < 16; bit++) {
            if (!((addr >> bit) & 1)) continue;
            if (bit == 8 && vregs.count(v)) {
                uses_scale = true;
                out.push_back("    v" + num(v) + " = asuint(IN.pos.x * ctr_scale.x);");
            }
            if (bit == 9 && vregs.count(v)) {
                uses_scale = true;
                out.push_back("    v" + num(v) + " = asuint(IN.pos.y * ctr_scale.x);");
            }
            if (bit == 10 && vregs.count(v)) out.push_back("    v" + num(v) + " = asuint(IN.pos.z);");
            if (bit == 11 && vregs.count(v)) out.push_back("    v" + num(v) + " = asuint(IN.pos.w);");
            if (bit == 12 && vregs.count(v)) out.push_back("    v" + num(v) + " = IN.ff ? 1u : 0u;");
            v += sizes[bit];
        }
        return out;
    }
};

inline uint64_t fnv1a64(const uint8_t* p, size_t n) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

// What the runtime needs besides the bytecode (renderer.cpp LoadShaderIndex).
struct Translation {
    std::string hlsl;
    std::vector<std::pair<int64_t, int>> textures; // sorted: slot, dims
    std::map<int64_t, std::string> samplers;
    std::set<int64_t> cbuffers;
};
inline Translation finish(Translator& t, std::string hlsl) {
    Translation r;
    r.hlsl = std::move(hlsl);
    for (auto& [slot, du] : t.textures) r.textures.push_back({slot, du.first});
    std::sort(r.textures.begin(), r.textures.end());
    r.samplers = t.samplers;
    r.cbuffers = t.cbuffers;
    return r;
}

// GS binary = GS program + footer, then the copy VS at the next 256-byte boundary
inline Translation translate_gs(const std::vector<uint8_t>& code, const GnmMeta& meta) {
    const int64_t p = find(code, "OrbShdr");
    if (p < 0 || p + 12 > (int64_t)code.size()) throw Unsupported("GS footer");
    uint32_t bits;
    memcpy(&bits, code.data() + p + 8, 4);
    const int64_t glen = bits >> 8;
    const int64_t cstart = (p / 256 + 1) * 256;
    const auto cinfo = parse_footer(code);
    if (!cinfo || !meta.copy) throw Unsupported("GS copy shader");
    auto cinsts = decode(code, cstart, cstart + cinfo->length);
    for (auto& i : cinsts) i.pc -= cstart;
    int64_t g = 0;
    bool any = false;
    for (const auto& i : cinsts)
        if (i.name == "buffer_load_dword" && i.I("offset")) {
            g = std::gcd(g, i.I("offset"));
            any = true;
        }
    if (!any) throw Unsupported("copy shader ring stride");
    const int64_t max_vert = g / 64;
    Translator copy("vs", cinsts, *meta.copy, cinfo->length, {}, Ring{"copy", max_vert, "", 0});
    const std::string copy_fn = copy.function_text("VSOut copyvs(uint gso[" + num(GSO_LEN) + "])");
    Ring ring{"gs", max_vert, copy_fn, std::min<int64_t>(max_vert, 1024 / GS_OUT_SCALARS)};
    const auto insts = decode(code, 0, glen);
    Translator t("gs", insts, meta, glen, {}, ring);
    std::string hlsl = t.translate();
    return finish(t, std::move(hlsl));
}

inline Translation translate_one(const std::vector<uint8_t>& code, const std::string& kind, const GnmMeta& meta) {
    if (kind == "gs") return translate_gs(code, meta);
    const auto info = parse_footer(code);
    if (!info) throw Unsupported("no footer");
    const auto insts = decode(code, 0, info->length);
    const Ring ring = kind == "es" ? Ring{"es", 0, "", 0} : Ring{};
    Translator t(kind, insts, meta, info->length, {}, ring);
    std::string hlsl = t.translate();
    std::map<int64_t, int> mixed;
    for (auto& [slot, d] : t.dims_seen)
        if (d.size() > 1) mixed[slot] = 3;
    if (!mixed.empty()) { // a slot sampled both as 2D and as cube/array: declare it as an array
        Translator t2(kind, insts, meta, info->length, mixed, ring);
        std::string h2 = t2.translate();
        return finish(t2, std::move(h2));
    }
    return finish(t, std::move(hlsl));
}

} // namespace gcn
