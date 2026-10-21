// SPDX-License-Identifier: GPL-2.0-only
// Checks of mod_overlay.hpp and guest_patch_xml.hpp on synthetic files in a temporary folder;
// no game files. Build and run: powershell -File ctrport/test_mods.ps1
#include "app0.hpp"
#include "guest_patch_xml.hpp"
#include <iostream>

namespace fs = std::filesystem;
using namespace ctrport;
static int failures = 0, passes = 0;
static void check(bool ok, const char* what) {
    std::cout << (ok ? "PASS " : "FAIL ") << what << "\n";
    ok ? ++passes : ++failures;
}
template <class F> static bool throws(F f, const char* fragment = "") {
    try { f(); } catch (const std::exception& e) { return std::string(e.what()).find(fragment) != std::string::npos; }
    return false;
}
static void put(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc) << data;
}
static std::string get(const fs::path& path) { return mods::read_text(path); }

int main() {
    const fs::path root = fs::temp_directory_path() / ("ctr_mods_test_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(root);
    const auto base = root / "game/CUSA14876", patch = root / "game/CUSA14876-patch", dir = root / "mods";
    put(base / "archives/a.pak", "base-a");
    put(base / "archives/b.pak", "base-b");
    put(base / "version.xml", "base-version");
    put(patch / "archives/a.pak", "patch-a");

    // --- mods.json ---------------------------------------------------------------------------
    check(mods::read_config(root / "missing.json").order.empty(), "missing mods.json means defaults");
    put(root / "m.json", "\xef\xbb\xbf{\"order\": [\"Zeta\", \"Caf\\u00e9\"], \"disabled\": [\"Zeta\"]}");
    const auto config = mods::read_config(root / "m.json");
    check(config.order.size() == 2 && config.order[1] == L"Café" && config.disabled == std::vector<std::wstring>{L"Zeta"},
          "mods.json order/disabled with BOM and \\u escape");
    put(root / "bad.json", "{\"order\": [1]}");
    check(throws([&] { mods::read_config(root / "bad.json"); }, "only strings"), "mods.json rejects non-string entries");
    mods::write_config(root / "w.json", config);
    const auto again = mods::read_config(root / "w.json");
    check(again.order == config.order && again.disabled == config.disabled, "mods.json round trip");

    // --- layouts and order -------------------------------------------------------------------
    put(dir / "Alpha/archives/a.pak", "alpha-a");                          // game folders directly
    put(dir / "beta/app0/archives/A.PAK", "beta-a");                       // app0/, other case
    put(dir / "beta/app0/archives/new.pak", "beta-new");
    put(dir / "Gamma/Gamma v1/CUSA14876/archives/b.pak", "gamma-b");       // wrapper + title folder
    put(dir / "Gamma/readme.txt", "notes");
    put(dir / "Delta/Delta/icons/x.png", "delta-icon");                    // wrapper only
    put(dir / "Delta/Delta/readme.md", "notes");
    fs::create_directories(dir / ".hidden/archives");
    check(mods::content_root(dir / "Alpha") == dir / "Alpha", "layout: game folders in the mod root");
    check(mods::content_root(dir / "beta") == dir / "beta/app0", "layout: app0 folder");
    check(mods::content_root(dir / "Gamma") == dir / "Gamma/Gamma v1/CUSA14876", "layout: wrapper and CUSA14876 folder");
    check(mods::content_root(dir / "Delta") == dir / "Delta/Delta", "layout: one wrapper folder");

    auto list = mods::list_mods(dir, {});
    check(list.size() == 4 && list[0].name == L"Alpha" && list[1].name == L"beta" && list[3].name == L"Gamma",
          "new mods appended alphabetically, case-insensitive, hidden skipped");
    list = mods::list_mods(dir, {{L"Gamma", L"Missing", L"Alpha"}, {L"Delta"}});
    check(list.size() == 4 && list[0].name == L"Gamma" && list[1].name == L"Alpha" && list[2].name == L"beta" && list[3].name == L"Delta" && !list[3].enabled,
          "configured order first, unknown names ignored, disabled kept in place");

    // Alpha then beta: beta (later) wins for archives/a.pak.
    auto index = mods::build_index(mods::list_mods(dir, {{L"Alpha", L"beta", L"Gamma"}, {L"Delta"}}), {patch, base});
    check(get(index.files.at(L"archives/a.pak")) == "beta-a", "later enabled mod wins");
    check(index.owner.at(L"archives/b.pak") == L"Gamma" && index.files.count(L"icons/x.png") == 0, "disabled mod contributes nothing");
    check(index.replaced == 2 && index.added == 1, "replaced and added files counted against patch and base");
    check(index.files.count(L"readme.txt") == 0, "readme files of a mod are not game files");
    bool override_logged = false;
    for (const auto& line : index.log) override_logged |= line == "Mods: archives/A.PAK: Alpha -> beta";
    check(override_logged, "override between mods is logged");

    // --- /app0 resolution: mods, then patch, then base ------------------------------------
    mods::overlay.configure(dir, root / "order.json", {patch, base});
    check(get(app0_read_path(base, patch, "archives/a.pak")) == "beta-a", "app0: mod over patch (default alphabetical order)");
    check(get(app0_read_path(base, patch, "archives/new.pak")) == "beta-new", "app0: file added by a mod");
    check(get(app0_read_path(base, patch, "version.xml")) == "base-version", "app0: unmodded file falls back to base");
    mods::write_config(root / "order.json", {{L"beta", L"Alpha"}, {L"beta", L"Gamma", L"Delta"}});
    mods::overlay.configure(dir, root / "order.json", {patch, base});
    check(get(app0_read_path(base, patch, "archives/a.pak")) == "alpha-a" && get(app0_read_path(base, patch, "archives/b.pak")) == "base-b",
          "app0: disabling mods restores the game files");
    mods::write_config(root / "order.json", {{}, {L"Alpha", L"beta", L"Gamma", L"Delta"}});
    mods::overlay.configure(dir, root / "order.json", {patch, base});
    check(get(app0_read_path(base, patch, "archives/a.pak")) == "patch-a", "app0: all mods disabled leaves patch over base");
    check(throws([&] { app0_read_path(base, patch, "../escape"); }, "traversal"), "app0: traversal still rejected");

    // --- unsupported mod content -------------------------------------------------------------
    put(root / "bad1/Code/eboot.bin", "x");
    check(throws([&] { mods::build_index(mods::list_mods(root / "bad1", {}), {}); }, "patch XML"), "mod replacing eboot.bin rejected");
    fs::create_directories(root / "bad2/Linked/archives");
    const auto target = root / "outside";
    fs::create_directories(target);
    // A junction needs no privilege, unlike a symbolic link.
    const auto junction = L"cmd /c mklink /J \"" + (root / "bad2/Linked/archives/link").wstring() + L"\" \"" + target.wstring() + L"\" >nul";
    check(_wsystem(junction.c_str()) == 0 && throws([&] { mods::build_index(mods::list_mods(root / "bad2", {}), {}); }, "links"),
          "mod containing a junction rejected");

    // --- patch XML -----------------------------------------------------------------------------
    using namespace guest_patches;
    const std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
<!-- synthetic -->
<Patch>
    <TitleID>
        <ID>CUSA14876</ID>
        <ID> CUSA13795 </ID>
    </TitleID>
    <Metadata Title="Test" Name="Speed &amp; Co" Note="n" Author="me" PatchVer="1.0" AppVer="01.21" AppElf="eboot.bin" isEnabled="true">
        <PatchList>
            <Line Type="bytes" Address="0x00401000" Value="90 90"/>
            <Line Type="bytes32" Address="0x401010" Value="0x11223344"/>
            <Line Type="bytes16" Address="0x401020" Value="258"/>
            <Line Type="float32" Address="0x401030" Value="1.5"/>
            <Line Type="utf8" Address="0x401040" Value="Hi"/>
            <Line Type="utf16" Address="0x401050" Value="Hi"/>
            <Line Type="bytes64" Address="0x401060" Value="0x0102030405060708"/>
        </PatchList>
    </Metadata>
    <Metadata Name="Old" AppVer="01.00" AppElf="eboot.bin">
        <PatchList><Line Type="bytes" Address="0x401000" Value="CC"/></PatchList>
    </Metadata>
    <Metadata Name="Anywhere" AppVer="01.00" AppElf="eboot.bin">
        <PatchList><Line Type="mask" Address="DE AD ?? EF" Offset="1" Value="77"/></PatchList>
    </Metadata>
    <Metadata Name="Outside" AppVer="01.21" AppElf="eboot.bin">
        <PatchList><Line Type="bytes" Address="0x500000" Value="00"/></PatchList>
    </Metadata>
    <Metadata Name="Cave" AppVer="01.21" AppElf="eboot.bin">
        <PatchList><Line Type="mask_jump32" Address="DE AD" Value="90"/></PatchList>
    </Metadata>
</Patch>)";
    const auto parsed = parse_patches(xml, L"ctr.xml");
    check(parsed.size() == 5 && parsed[0].name == "Speed & Co" && parsed[0].title_ids.size() == 2 && parsed[0].title_ids[1] == "CUSA13795" &&
          parsed[0].default_enabled && !parsed[1].default_enabled && parsed[0].lines.size() == 7, "patch XML parsed (entities, IDs, isEnabled)");
    check(parsed[0].key() == L"ctr.xml/Speed & Co", "patch key is file/Name as in patches.json");
    check(encode("bytes32", "0x11223344") == std::vector<unsigned char>{0x44, 0x33, 0x22, 0x11}, "bytes32 little-endian");
    check(encode("bytes16", "258") == std::vector<unsigned char>{2, 1}, "bytes16 decimal");
    check(encode("float32", "1.5") == std::vector<unsigned char>{0, 0, 0xc0, 0x3f}, "float32 little-endian");
    check(encode("utf8", "Hi") == std::vector<unsigned char>{'H', 'i', 0}, "utf8 terminated");
    check(encode("utf16", "Hi") == std::vector<unsigned char>{'H', 0, 'i', 0, 0, 0}, "utf16 terminated");
    check(throws([] { encode("bytes16", "0x10000"); }, "does not fit"), "bytes16 overflow rejected");
    check(throws([] { encode("bytes", "123"); }, "even"), "odd hex digits rejected");
    check(throws([] { encode("bytes", "zz"); }, "hexadecimal"), "non-hex bytes rejected");
    check(throws([] { parse_patches("<Patch><Metadata Name='x'></Patch>", L"bad.xml"); }, "mismatched"), "malformed XML rejected");
    check(mismatch(parsed[0], "CUSA14876", "01.21").empty() && mismatch(parsed[0], "CUSA13795", "01.21").empty(), "patch matches listed titles");
    check(!mismatch(parsed[0], "CUSA99999", "01.21").empty() && !mismatch(parsed[1], "CUSA14876", "01.21").empty(), "other title or AppVer skipped");
    check(mismatch(parsed[2], "CUSA14876", "01.21").empty(), "mask-only patch applies to any AppVer (shadPS4)");

    std::vector<unsigned char> image(0x2000, 0);
    image[0x1800] = 0xde; image[0x1801] = 0xad; image[0x1802] = 0x55; image[0x1803] = 0xef;
    const Spans spans{{0x1000, 0x2000}};
    const auto writes = plan(parsed[0], image.data(), spans, true);
    check(writes.size() == 7 && writes[0].offset == 0x1000 && writes[0].bytes == std::vector<unsigned char>{0x90, 0x90},
          "address includes the 0x400000 eboot base");
    const auto masked = plan(parsed[2], image.data(), spans, false);
    check(masked.size() == 1 && masked[0].offset == 0x1801, "mask pattern with wildcard found, Offset applied");
    check(throws([&] { plan(parsed[3], image.data(), spans, true); }, "outside"), "write outside mapped segments rejected");
    check(throws([&] { plan(parsed[4], image.data(), spans, true); }, "mask_jump32"), "mask_jump32 reported as unsupported");
    check(throws([&] { plan(parsed[0], image.data(), Spans{{0x1000, 0x1001}}, true); }, "outside"), "write crossing a segment end rejected");

    // --- patches.json and the runtime entry point --------------------------------------------
    const auto patches = root / "patches";
    put(patches / "ctr.xml", xml);
    put(patches / "broken.xml", "<Patch>");
    Selection selection;
    choose(selection, parsed[3], false);  // "Outside" stays off
    choose(selection, parsed[1], true);   // "Old" on, but AppVer 01.00: skipped
    write_selection(root / "patches.json", selection);
    const auto read_back = read_selection(root / "patches.json");
    check(read_back.enabled == std::vector<std::wstring>{L"ctr.xml/Old"} && read_back.disabled == std::vector<std::wstring>{L"ctr.xml/Outside"},
          "patches.json round trip");
    check(selected(parsed[0], read_back) && selected(parsed[1], read_back) && !selected(parsed[3], read_back) && !selected(parsed[2], read_back),
          "selection: explicit choices over isEnabled default");
    std::vector<unsigned char> game(0x2000, 0);
    const int applied = apply_selected(game.data(), spans, "CUSA14876", "01.21", patches, root / "patches.json");
    std::uint32_t word = 0; std::memcpy(&word, game.data() + 0x1010, 4);
    check(applied == 1 && game[0x1000] == 0x90 && word == 0x11223344, "runtime applies the selected matching patch only");
    selection.enabled.push_back(L"broken.xml/Anything");
    write_selection(root / "patches.json", selection);
    check(throws([&] { apply_selected(game.data(), spans, "CUSA14876", "01.21", patches, root / "patches.json"); }, "cannot be read"),
          "selected patch in an unreadable file stops the launch");
    choose(selection, parsed[3], true);
    selection.enabled.erase(std::remove(selection.enabled.begin(), selection.enabled.end(), L"broken.xml/Anything"), selection.enabled.end());
    write_selection(root / "patches.json", selection);
    std::vector<unsigned char> untouched(0x2000, 0);
    check(throws([&] { apply_selected(untouched.data(), spans, "CUSA14876", "01.21", patches, root / "patches.json"); }, "Outside"),
          "selected patch with an invalid address stops the launch");
    check(std::all_of(untouched.begin(), untouched.end(), [](unsigned char b) { return b == 0; }),
          "nothing is written when any selected patch is invalid");

    // --- param.sfo ---------------------------------------------------------------------------
    std::string sfo("\0PSF\x01\x01\0\0", 8);
    const auto u32 = [](std::uint32_t v) { return std::string{char(v), char(v >> 8), char(v >> 16), char(v >> 24)}; };
    const std::string keys = std::string("APP_VER\0TITLE_ID\0", 17);
    const std::uint32_t key_table = 20 + 2 * 16, value_table = key_table + 20;
    sfo += u32(key_table) + u32(value_table) + u32(2);
    sfo += u32(0 | (0x0204u << 16)) + u32(6) + u32(8) + u32(0);
    sfo += u32(8 | (0x0204u << 16)) + u32(10) + u32(12) + u32(8);
    sfo += keys + std::string(3, '\0');
    sfo += std::string("01.21\0\0\0", 8) + std::string("CUSA14876\0\0\0", 12);
    put(root / "param.sfo", sfo);
    const auto values = read_sfo(root / "param.sfo");
    check(values.at("APP_VER") == "01.21" && values.at("TITLE_ID") == "CUSA14876", "param.sfo TITLE_ID and APP_VER");

    std::error_code ec;
    fs::remove_all(root, ec);
    std::cout << passes << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
