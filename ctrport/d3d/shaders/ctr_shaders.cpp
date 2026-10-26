// SPDX-License-Identifier: GPL-2.0-or-later
// ctr_shaders.exe: builds the translated shader set of the Direct3D 11 renderer from the user's own
// dump (nothing of the game is shipped): runtime/user/gpu_d3d/shaders.bin.
//
//   1. reads the shader IGZ files of <base>/archives/shaders_ps4.pak and of the patch archives
//      that carry shader buffers,
//   2. exports every GCN shader with its Gnm struct,
//   3. adds the shaders ctr_d3d met at run time (<user>/missing_shaders),
//   4. translates GCN to HLSL (gcn_hlsl.hpp) and compiles it with d3dcompiler_47,
//   5. packs the result: "CTRS", u32 version 1, u32 index length, index text, u32 count,
//      count x {u16 name length, name, u32 size, DXBC}, names sorted.
//
// usage: ctr_shaders --base <CUSA14876> [--patch <CUSA14876-patch>] [--user <runtime/user/gpu_d3d>]
//                    [--out <shaders.bin>] [--hlsl <dir>] [--jobs <n>]
// --hlsl writes the translated sources, to read or to compare. Compiled bytecode is kept in
// <user>/dxbc_cache by HLSL hash: a rebuild (new shaders met at run time) compiles only new ones. Progress goes to stdout as
// "progress <done> <total>" lines; the exit code is 0 when shaders.bin was written.
#include <windows.h>
#include <d3dcommon.h>

#include <cstdarg>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "../../settings/menu_assets.hpp"
#include "shader_export.hpp"

namespace fs = std::filesystem;
using gcn::Bytes;

namespace {

using D3DCompileFn = HRESULT(WINAPI*)(const void*, SIZE_T, const char*, const void*, void*, const char*, const char*, UINT,
                                      UINT, ID3DBlob**, ID3DBlob**);
D3DCompileFn g_compile;

std::mutex g_out;
void say(const char* fmt, ...) {
    std::lock_guard lk(g_out);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

// extracted path of an archive entry, the order the files are exported in
std::string entry_key(const std::string& prefix, const ctrport::settings::menu_assets::IgaEntry& e) {
    std::string rel = e.path.empty() ? e.full_path : e.path;
    for (char& c : rel)
        if (c == '/') c = '\\';
        else if (c == ':') c = '_';
    while (!rel.empty() && rel[0] == '\\') rel.erase(0, 1);
    return prefix + rel;
}

struct IgzSource {
    std::string effect;
    std::vector<gcn::ShaderEntry> shaders;
};

// the shader buffers of the IGZ files of one archive; returns the number of files used
// (the IGZ files are decompressed on `jobs` threads, each with its own archive handle)
int read_archive(const fs::path& pak, const std::string& prefix, bool only_shaders, unsigned jobs,
                 std::map<std::string, IgzSource>& files) {
    using ctrport::settings::menu_assets::Iga;
    const Iga directory(pak, false);
    std::vector<size_t> todo;
    for (size_t k = 0; k < directory.files.size(); k++) {
        const auto& e = directory.files[k];
        std::string lower = e.path.empty() ? e.full_path : e.path;
        for (char& c : lower) c = char(tolower((unsigned char)c));
        if (lower.size() >= 4 && !lower.compare(lower.size() - 4, 4, ".igz")) todo.push_back(k);
    }
    enum State { Skipped, Unreadable, Used };
    std::vector<State> state(todo.size(), Skipped);
    std::vector<IgzSource> found(todo.size());
    std::atomic<size_t> next{0}, finished{0};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < jobs; t++)
        pool.emplace_back([&] {
            Iga a(pak, false);
            for (size_t k; (k = next++) < todo.size();) {
                const auto& e = a.files[todo[k]];
                Bytes data;
                try {
                    data = a.extract(e);
                } catch (const std::exception&) {  // block formats the archive reader does not know
                    state[k] = Unreadable;
                    continue;
                }
                static const char tag[] = "igShaderBuffer";
                if (only_shaders && std::search(data.begin(), data.end(), tag, tag + sizeof(tag) - 1) == data.end()) continue;
                const std::string key = entry_key(prefix, e);
                std::string effect = key.substr(key.rfind('\\') + 1);
                effect.resize(effect.size() - 4);
                found[k].effect = effect;
                try {
                    gcn::export_igz(gcn::IgzFile(std::move(data)), effect, found[k].shaders);
                } catch (const std::exception& ex) {
                    say("     %s: %s, skipped", key.c_str(), ex.what());
                }
                state[k] = Used;
                const size_t n = ++finished;
                if (n % 500 == 0) say("read %zu %zu", n, todo.size());
            }
        });
    for (auto& t : pool) t.join();
    int n = 0, unreadable = 0;
    for (size_t k = 0; k < todo.size(); k++) {  // archive order: a later entry with the same path wins
        if (state[k] == Unreadable) unreadable++;
        if (state[k] != Used) continue;
        files[entry_key(prefix, directory.files[todo[k]])] = std::move(found[k]);
        n++;
    }
    if (unreadable) say("     %s: %d files not readable, skipped", pak.filename().string().c_str(), unreadable);
    return n;
}

std::string blob_text(ID3DBlob* b) { return b ? std::string((const char*)b->GetBufferPointer(), b->GetBufferSize()) : ""; }

struct Result {
    enum { Compiled, Unsupported, CompileError } status = Unsupported;
    std::string reason;
    std::vector<std::string> lines;
    Bytes dxbc;
};

// records read by ctrport/d3d/renderer.cpp LoadShaderIndex
std::vector<std::string> index_lines(const gcn::ShaderEntry& e, const gcn::Translation& t) {
    char h[32];
    snprintf(h, sizeof(h), "%016llx",
             (unsigned long long)gcn::fnv1a64(e.code.data(), std::min<size_t>(e.code.size(), e.gnm.shader_size)));
    std::vector<std::string> out = {std::string("S ") + h + " " + e.name + " " + e.kind};
    for (const auto& [slot, dims] : t.textures) out.push_back("T " + std::to_string(slot) + " " + std::to_string(dims));
    for (const auto& [slot, kind] : t.samplers) out.push_back("M " + std::to_string(slot) + " " + kind);
    for (int64_t slot : t.cbuffers) out.push_back("B " + std::to_string(slot));
    if (e.kind == "vs" || e.kind == "es")
        for (const auto& in : e.gnm.inputs) out.push_back("I " + std::to_string(in.sem) + " " + std::to_string(in.size));
    return out;
}

std::atomic<uint64_t> g_translate_ms{0}, g_compile_ms{0}, g_cached{0};  // summed over the worker threads
fs::path g_cache;  // <user>/dxbc_cache: compiled bytecode by HLSL hash, so a rebuild compiles only new shaders

Result work(const gcn::ShaderEntry& e, const fs::path& hlsl_dir) {
    const ULONGLONG t0 = GetTickCount64();
    Result r;
    gcn::Translation t;
    try {
        t = gcn::translate_one(e.code, e.kind, e.gnm);
    } catch (const std::exception& ex) {
        r.reason = ex.what();
        return r;
    }
    if (!hlsl_dir.empty()) std::ofstream(hlsl_dir / (e.name + ".hlsl"), std::ios::binary) << t.hlsl;
    const char* target = e.kind == "vs" || e.kind == "es" ? "vs_5_0" : e.kind == "gs" ? "gs_5_0" : "ps_5_0";
    const ULONGLONG t1 = GetTickCount64();
    g_translate_ms += t1 - t0;
    char leaf[64];
    snprintf(leaf, sizeof(leaf), "%016llx_%s.dxbc", (unsigned long long)gcn::fnv1a64((const uint8_t*)t.hlsl.data(), t.hlsl.size()),
             target);
    const fs::path cached = g_cache / leaf;
    if (std::ifstream f{cached, std::ios::binary}) {
        r.dxbc.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        if (!r.dxbc.empty()) {
            g_cached++;
            r.status = Result::Compiled;
            r.lines = index_lines(e, t);
            return r;
        }
    }
    ID3DBlob *code = nullptr, *err = nullptr;
    const HRESULT hr = g_compile(t.hlsl.data(), t.hlsl.size(), nullptr, nullptr, nullptr, "main", target, 1 << 14 | 1 << 11, 0,
                                 &code, &err);
    g_compile_ms += GetTickCount64() - t1;
    const std::string msg = blob_text(err);
    if (err) err->Release();
    if (FAILED(hr) || !code) {
        if (code) code->Release();
        r.status = Result::CompileError;
        r.reason = msg.substr(0, 200);
        for (const auto& l : gcn::split(msg, '\n'))
            if (l.find("error") != std::string::npos) {
                r.reason = l;
                break;
            }
        return r;
    }
    r.dxbc.assign((const uint8_t*)code->GetBufferPointer(), (const uint8_t*)code->GetBufferPointer() + code->GetBufferSize());
    code->Release();
    {
        fs::path tmp = cached;
        tmp += ".tmp" + std::to_string(GetCurrentThreadId());
        std::ofstream(tmp, std::ios::binary).write((const char*)r.dxbc.data(), r.dxbc.size());
        std::error_code ec;
        fs::rename(tmp, cached, ec);
    }
    r.status = Result::Compiled;
    r.lines = index_lines(e, t);
    return r;
}

void put32(Bytes& b, uint32_t v) { b.insert(b.end(), (const uint8_t*)&v, (const uint8_t*)&v + 4); }

int run(int argc, wchar_t** argv) {
    fs::path base, patch, user = "runtime/user/gpu_d3d", out, hlsl_dir;
    unsigned jobs = std::max(1u, std::thread::hardware_concurrency());
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::wstring k = argv[i];
        if (k == L"--base") base = argv[i + 1];
        else if (k == L"--patch") patch = argv[i + 1];
        else if (k == L"--user") user = argv[i + 1];
        else if (k == L"--out") out = argv[i + 1];
        else if (k == L"--hlsl") hlsl_dir = argv[i + 1];
        else if (k == L"--jobs") jobs = std::max(1, _wtoi(argv[i + 1]));
        else {
            say("unknown option %ls", k.c_str());
            return 2;
        }
    }
    if (base.empty()) {
        say("usage: ctr_shaders --base <CUSA14876> [--patch <dir>] [--user <dir>] [--out <file>] [--hlsl <dir>] [--jobs <n>]");
        return 2;
    }
    if (out.empty()) out = user / "shaders.bin";
    HMODULE dll = LoadLibraryW(L"d3dcompiler_47.dll");
    g_compile = dll ? (D3DCompileFn)GetProcAddress(dll, "D3DCompile") : nullptr;
    if (!g_compile) {
        say("error: d3dcompiler_47.dll not found");
        return 3;
    }
    std::error_code ec;
    if (!hlsl_dir.empty()) fs::create_directories(hlsl_dir, ec);
    g_cache = user / "dxbc_cache";
    fs::create_directories(g_cache, ec);

    const ULONGLONG start = GetTickCount64();
    const fs::path pak = base / "archives" / "shaders_ps4.pak";
    say("step 1 4 reading %s", pak.string().c_str());
    std::map<std::string, IgzSource> files;
    say("     %d shader files", read_archive(pak, "", false, jobs, files));
    if (!patch.empty() && fs::is_directory(patch / "archives", ec)) {
        std::vector<fs::path> paks;
        for (const auto& it : fs::directory_iterator(patch / "archives", ec))
            if (it.path().extension() == ".pak") paks.push_back(it.path());
        std::sort(paks.begin(), paks.end());
        for (const auto& p : paks) {
            try {
                const int n = read_archive(p, "patch_" + p.stem().string() + "\\", true, jobs, files);
                if (n) say("     %d shader files from %s", n, p.filename().string().c_str());
            } catch (const std::exception& ex) {
                say("     %s: %s, skipped", p.filename().string().c_str(), ex.what());
            }
        }
    }
    std::vector<gcn::ShaderEntry> index;
    for (auto& [key, src] : files)
        for (auto& s : src.shaders) index.push_back(std::move(s));
    files.clear();

    say("step 2 4 shaders met at run time");
    std::vector<std::string> log;
    const int added = fs::is_directory(user / "missing_shaders", ec) ? gcn::import_runtime(user / "missing_shaders", index, log) : 0;
    for (const auto& l : log) say("     %s", l.c_str());
    say("     %d runtime shaders added", added);

    std::vector<const gcn::ShaderEntry*> todo;
    std::set<std::string> done;
    for (const auto& e : index) {
        if (done.count(e.name) || !(e.kind == "vs" || e.kind == "ps" || e.kind == "es" || e.kind == "gs")) continue;
        done.insert(e.name);
        todo.push_back(&e);
    }
    say("step 3 4 translating %zu shaders", todo.size());
    std::vector<Result> results(todo.size());
    std::atomic<size_t> next{0}, finished{0};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < jobs; t++)
        pool.emplace_back([&] {
            for (size_t k; (k = next++) < todo.size();) {
                results[k] = work(*todo[k], hlsl_dir);
                const size_t n = ++finished;
                if (n % 50 == 0 || n == todo.size()) say("progress %zu %zu", n, todo.size());
            }
        });
    for (auto& t : pool) t.join();

    say("     %llu from the bytecode cache; thread time: translation %llu s, compilation %llu s", (unsigned long long)g_cached,
        g_translate_ms / 1000, g_compile_ms / 1000);
    say("step 4 4 packing");
    std::map<std::string, int> unsupported, compile_errors;
    std::string text;
    std::map<std::string, const Bytes*> blobs;
    int compiled = 0;
    for (size_t k = 0; k < todo.size(); k++) {
        const Result& r = results[k];
        if (r.status == Result::Compiled) {
            compiled++;
            for (const auto& l : r.lines) text += l + "\n";
            blobs[todo[k]->name] = &r.dxbc;
        } else if (r.status == Result::Unsupported) {
            unsupported[r.reason.substr(0, 60)]++;
        } else {
            compile_errors[r.reason.substr(0, 120)]++;
        }
    }
    Bytes pack = {'C', 'T', 'R', 'S'};
    put32(pack, 1);
    put32(pack, uint32_t(text.size()));
    pack.insert(pack.end(), text.begin(), text.end());
    put32(pack, uint32_t(blobs.size()));
    for (const auto& [name, data] : blobs) {
        const uint16_t n = uint16_t(name.size());
        pack.insert(pack.end(), (const uint8_t*)&n, (const uint8_t*)&n + 2);
        pack.insert(pack.end(), name.begin(), name.end());
        put32(pack, uint32_t(data->size()));
        pack.insert(pack.end(), data->begin(), data->end());
    }
    fs::create_directories(out.parent_path(), ec);
    fs::path tmp = out;
    tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary);
        f.write((const char*)pack.data(), pack.size());
        if (!f) {
            say("error: cannot write %s", tmp.string().c_str());
            return 4;
        }
    }
    fs::rename(tmp, out, ec);
    if (ec) {
        say("error: cannot replace %s: %s", out.string().c_str(), ec.message().c_str());
        return 4;
    }
    say("%zu shaders: %d compiled, %d not translatable, %d not compiled", todo.size(), compiled,
        int(todo.size()) - compiled - [&] { int n = 0; for (auto& [k, v] : compile_errors) n += v; return n; }(),
        [&] { int n = 0; for (auto& [k, v] : compile_errors) n += v; return n; }());
    for (const auto& [k, v] : unsupported) say("     not translatable %4d  %s", v, k.c_str());
    for (const auto& [k, v] : compile_errors) say("     not compiled     %4d  %s", v, k.c_str());
    say("done %s (%zu MiB, %llu s)", out.string().c_str(), pack.size() >> 20, (GetTickCount64() - start) / 1000);
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& ex) {
        say("error: %s", ex.what());
        return 1;
    }
}
