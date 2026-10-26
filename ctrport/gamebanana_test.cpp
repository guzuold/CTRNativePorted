// SPDX-License-Identifier: GPL-2.0-only
// Checks of gamebanana.hpp on synthetic API answers and synthetic extracted downloads in a
// temporary folder; no network, no game files. Build and run: powershell -File ctrport/test_mods.ps1
#include "app0.hpp"
#include "gamebanana.hpp"
#include "trophy_wiki.hpp"
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

int main() {
    namespace gb = gamebanana;
    // --- API answers -----------------------------------------------------------------------------
    const auto feed = ctr_online::parse_json(R"J({"_aMetadata":{"_nRecordCount":4,"_bIsComplete":true},"_aRecords":[
        {"_idRow":1,"_sModelName":"Mod","_sName":"Classic SFX (Switch)","_bHasFiles":true,"_aSubmitter":{"_sName":"A"},"_sProfileUrl":"https://gamebanana.com/mods/1"},
        {"_idRow":2,"_sModelName":"Sound","_sName":"Coco Voice (PS4)","_bHasFiles":true,"_aSubmitter":{"_sName":"B"}},
        {"_idRow":3,"_sModelName":"Request","_sName":"Harder AI","_bHasFiles":false},
        {"_idRow":4,"_sModelName":"Tool","_sName":"Mod Kit","_bHasFiles":true}]})J");
    bool complete = false;
    const auto entries = gb::parse_subfeed(feed, gb::kConsoleGame, &complete);
    check(complete && entries.size() == 2, "subfeed keeps mods and sounds with files");
    check(entries.size() == 2 && entries[0].platform == L"Switch" && entries[1].platform == L"PS4" && entries[0].author == "A",
          "subfeed platform from the name, submitter");
    check(gb::parse_subfeed(feed, gb::kPcGame)[1].platform == L"PC", "PC game entries are PC");
    const auto files = gb::parse_download_page(ctr_online::parse_json(R"J({"_aFiles":[
        {"_idRow":9,"_sFile":"a.zip","_nFilesize":1234,"_sMd5Checksum":"00","_sDownloadUrl":"https://gamebanana.com/dl/9"},
        {"_idRow":10,"_sFile":"b.zip","_nFilesize":1,"_sDownloadUrl":"http://elsewhere.example/b.zip"}]})J"));
    check(files.size() == 1 && files[0].id == 9 && files[0].size == 1234, "download page keeps only gamebanana.com https files");

    // --- Installation ----------------------------------------------------------------------------
    const fs::path root = fs::temp_directory_path() / ("ctr_gamebanana_test_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(root);
    const auto mods_dir = root / "mods", patches_dir = root / "patches";

    // PS4 layout with a wrapper and a readme.
    const auto a = root / "a";
    put(a / "Coco/CUSA14876/archives/drivercoco.pak", "pak");
    put(a / "Coco/readme.txt", "hi");
    const auto one = gb::install_extracted(a, mods_dir, patches_dir, L"gb7-", L"Coco: Voice (PS4)");
    check(one.mods.size() == 1 && one.mods[0] == L"Coco_ Voice (PS4)" && one.patches.empty(), "game folder installed as one mod, name made safe");
    check(fs::exists(mods_dir / one.mods[0] / "archives/drivercoco.pak") && mods::content_root(mods_dir / one.mods[0]) == mods_dir / one.mods[0],
          "mod folder holds the game folders at its root");

    // Switch layout with two variants.
    const auto b = root / "b";
    put(b / "0100F9F00C696000/Classic SFX/romfs/archives/update2.pak", "x");
    put(b / "0100F9F00C696000/Lite/romfs/archives/update2.pak", "y");
    put(b / "README.txt", "r");
    const auto two = gb::install_extracted(b, mods_dir, patches_dir, L"gb8-", L"Classic SFX");
    check(two.mods.size() == 2 && std::count(two.mods.begin(), two.mods.end(), L"Classic SFX - Lite") == 1 &&
          std::count(two.mods.begin(), two.mods.end(), L"Classic SFX - Classic SFX") == 1, "variants become one mod each, named after their folder");

    // Plain patch mod (patches/*.xml, no manifest); a non-patch XML is ignored.
    const auto c = root / "c";
    put(c / "patches/PhotoMode.xml", "<?xml version='1.0'?>\n<Patch>\n<TitleID><ID>CUSA13795</ID></TitleID>\n</Patch>\n");
    put(c / "other.xml", "<config/>");
    const auto three = gb::install_extracted(c, mods_dir, patches_dir, L"gb9-", L"Photo Mode");
    check(three.mods.empty() && three.patches.size() == 1 && three.patches[0] == L"gb9-PhotoMode.xml" && fs::exists(patches_dir / "gb9-PhotoMode.xml"),
          "patch XML copied to the patches folder with the download id");

    // Loose .pak files (Rayman, Sonic): installed under archives/.
    const auto f = root / "f";
    put(f / "Rayman (CTR NF)/driverrayman.pak", "r");
    put(f / "Rayman (CTR NF)/update3.pak", "u");
    const auto paks = gb::install_extracted(f, mods_dir, patches_dir, L"gb12-", L"Rayman");
    check(paks.mods.size() == 1 && fs::exists(mods_dir / "Rayman/archives/driverrayman.pak") && fs::exists(mods_dir / "Rayman/archives/update3.pak"),
          "loose .pak files go to archives/");
    // octr-mod (mod.json): the folder is installed whole, deep payload paths included; the
    // runtime applies it (octr_mods.hpp), so its patches stay inside the mod.
    const auto h = root / "h";
    const std::string deep = "payloads/assets/juicedomain_hub.pak/textures/GuiStandard_diffuse,textures@!ui!Octane!Boost!octane_ui_hud_boost_bg`tga,001_and_a_long_name_to_pass_max_path_for_sure_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.igz";
    put(h / "Gauge v1/mod.json", R"J({"schemaVersion":2,"requiresCapabilities":["archive-assets","xml-patches"],"operations":[{"kind":"archive_asset"}],"patches":["patches/reserves.xml"]})J");
    put(gb::long_path(h / "Gauge v1" / fs::path(deep)), "x");
    put(h / "Gauge v1/patches/reserves.xml", "<Patch></Patch>");
    const auto gauge = gb::install_extracted(h, mods_dir, patches_dir, L"gb13-", L"Gauge");
    check(gauge.mods.size() == 1 && gauge.patches.empty() && fs::exists(mods_dir / "Gauge/mod.json") &&
          fs::exists(gb::long_path(mods_dir / "Gauge" / fs::path(deep))) && fs::exists(mods_dir / "Gauge/patches/reserves.xml"),
          "octr-mod folder installed whole, payloads past MAX_PATH included");
    check(gb::read_manifest(mods_dir / "Gauge/mod.json").operations == 1 && gb::read_manifest(mods_dir / "Gauge/mod.json").patches == 1,
          "manifest summary counts operations and patches");
    gb::remove_installed(gauge, mods_dir, patches_dir);
    check(!fs::exists(mods_dir / "Gauge"), "octr-mod folder removed, long paths included");
    put(h / "Gauge v1/mod.json", R"J({"requiresCapabilities":["shaders"],"operations":[{"kind":"archive_asset"}]})J");
    check(throws([&] { gb::install_extracted(h, mods_dir, patches_dir, L"gb13-", L"Gauge"); }, "shaders") && !fs::exists(mods_dir / "Gauge"),
          "unsupported capability refused, nothing installed");
    put(h / "Gauge v1/mod.json", R"J({"operations":[{"kind":"shader"}]})J");
    check(throws([&] { gb::install_extracted(h, mods_dir, patches_dir, L"gb13-", L"Gauge"); }, "operation shader"), "unsupported operation kind refused");
    const auto n = root / "n";
    put(n / "nitro-mod.json", R"J({"requiresCapabilities":["xml-patches","shaders","variants"]})J");
    put(n / "patches/uw.xml", "<Patch></Patch>");
    check(throws([&] { gb::install_extracted(n, mods_dir, patches_dir, L"gb14-", L"UW"); }, "shaders, variants"), "nitro-mod.json with shaders refused");
    gb::remove_installed(paks, mods_dir, patches_dir);

    // Reinstall replaces the earlier folders instead of adding a copy.
    const auto again = gb::install_extracted(a, mods_dir, patches_dir, L"gb7-", L"Coco: Voice (PS4)", &one);
    check(again.mods == one.mods, "reinstall replaces the earlier install");
    // Name taken by a folder of the user: a numbered name.
    const auto other = gb::install_extracted(a, mods_dir, patches_dir, L"gb70-", L"Coco: Voice (PS4)");
    check(other.mods.size() == 1 && other.mods[0] == L"Coco_ Voice (PS4) (2)", "existing folder of another mod is kept");

    // Refusals, before anything is written.
    const auto d = root / "d";
    put(d / "readme.txt", "nothing");
    check(throws([&] { gb::install_extracted(d, mods_dir, patches_dir, L"gb10-", L"Empty"); }, "no mod.json"), "nothing installable is refused");
    const auto e = root / "e";
    put(e / "CUSA14876/eboot.bin", "elf");
    put(e / "CUSA14876/archives/a.pak", "a");
    check(throws([&] { gb::install_extracted(e, mods_dir, patches_dir, L"gb11-", L"Code"); }, "game code") && !fs::exists(mods_dir / "Code"),
          "eboot.bin is refused and nothing is written");

    // Registry round trip and removal.
    std::map<std::uint64_t, gb::Installed> installed{{7, again}, {9, three}};
    installed[7].file = "coco.zip";
    gb::write_installed(root / "gamebanana.json", installed);
    const auto read = gb::read_installed(root / "gamebanana.json");
    check(read.size() == 2 && read.at(7).mods == again.mods && read.at(7).file == "coco.zip" && read.at(9).patches == three.patches,
          "gamebanana.json round trip");
    gb::remove_installed(read.at(7), mods_dir, patches_dir);
    gb::remove_installed(read.at(9), mods_dir, patches_dir);
    check(!fs::exists(mods_dir / again.mods[0]) && !fs::exists(patches_dir / three.patches[0]) && fs::exists(mods_dir / other.mods[0]),
          "removal deletes only the recorded names");
    gb::remove_installed(gb::Installed{"", {L"..\\patches"}, {L"../x"}}, mods_dir, patches_dir);
    check(fs::exists(patches_dir), "recorded names with separators are ignored");

    check(gb::size_text(1536) == L"2 KB" && gb::size_text(7751189) == L"7.4 MB", "size text");

    // --- Trophy texts from the wiki (trophy_wiki.hpp) ----------------------------------------
    {
        namespace wiki = ctr_trophy::wiki;
        const std::string table = R"W(==Trophies/Achievements==
{|class="article-table"
|+
!Trophy Name
!Image
!Trophy Value
!Description
!Criteria
|-
|Woah!
|[[File:1Lcae65b.png|60px]]
|[[File:IMG 4262.PNG]]
|Earn all the other trophies.
|Earn every Trophy in the game.
|-
|Spare Change
|[[File:10L81ad53.png|60px]]
|[[File:IMG 4263.PNG]]
|Win a CTR challenge in the ''Adventure Mode''.
|Win a CTR Challenge in Crash Cove, or any track in [[N. Sanity Island]].<br>Also [[Gem Cup|Gem Cups]] &amp; more.
|}
)W";
        const auto rows = wiki::parse_table(table, {{"IMG 4262.PNG", 'P'}, {"IMG 4263.PNG", 'B'}});
        check(rows.size() == 2 && rows[0].name == "Woah!" && rows[0].grade == 'P' && rows[1].grade == 'B', "wiki table rows and grades");
        check(rows.size() == 2 && rows[1].detail == "Win a CTR challenge in the Adventure Mode." &&
              rows[1].tip == "Win a CTR Challenge in Crash Cove, or any track in N. Sanity Island. Also Gem Cups & more.",
              "wiki markup to plain text: links, italics, line breaks, entities");
        check(wiki::grade_of("File:Platinum Trophy.PNG") == 'P' && wiki::grade_of("File:Bronze_Trophy.png") == 'B' && wiki::grade_of("File:x.png") == 0,
              "grade from the redirect target");
        std::vector<std::string> values = {"P.png", "G.png", "G.png"};
        values.insert(values.end(), 10, "IMG 4264.PNG");
        values.insert(values.end(), 35, "IMG 4263.PNG");
        std::map<std::string, char> grades = {{"P.png", 'P'}, {"G.png", 'G'}, {"IMG 4263.PNG", '\0'}, {"IMG 4264.PNG", '\0'}};
        wiki::complete_grades(values, grades);
        check(grades["IMG 4263.PNG"] == 'B' && grades["IMG 4264.PNG"] == 'S', "bronze and silver from frequency, checked by points (1185)");
        std::vector<std::string> odd = {"P.png", "A.png", "B.png", "B.png"};
        std::map<std::string, char> unsure = {{"P.png", 'P'}};
        wiki::complete_grades(odd, unsure);
        check(!unsure.count("A.png") && !unsure.count("B.png"), "grades left unknown when the points do not fit");
        ctr_trophy::Set set;
        set.trophies = {{0}, {1}};
        check(wiki::apply(set, rows) && set.trophies[1].name == "Spare Change" && !set.trophies[1].tip.empty() && set.platinum() == 0,
              "wiki rows fill a set read without the key");
        ctr_trophy::Set bigger;
        bigger.trophies = {{0}, {1}, {2}};
        check(!wiki::apply(bigger, rows) && bigger.trophies[0].name.empty(), "a table of another size is not used");
        ctr_trophy::Set keyed;
        keyed.trophies = {{0}, {1}};
        keyed.texts = true;
        check(!wiki::apply(keyed, rows), "the key's own texts are kept");
        const auto cache = fs::temp_directory_path() / ("ctr_wiki_test_" + std::to_string(GetCurrentProcessId()) + ".json");
        wiki::write_cache(cache, rows);
        std::int64_t fetched = 0;
        const auto again_rows = wiki::read_cache(cache, &fetched);
        check(again_rows.size() == 2 && again_rows[1].tip == rows[1].tip && again_rows[0].grade == 'P' && fetched > 0, "wiki.json round trip");
        fs::remove(cache);
    }
    fs::remove_all(gb::long_path(root));
    std::cout << passes << " passed, " << failures << " failed\n";
    return failures ? 1 : 0;
}
