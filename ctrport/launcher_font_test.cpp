// SPDX-License-Identifier: GPL-2.0-only
// Local retail-data checks. Supply your own dump; generated preview stays local.
#include "launcher_font.hpp"
#include <iostream>
int wmain(int argc,wchar_t** argv) {
 if(argc!=4) { std::cerr << "Usage: launcher_font_test BASE PATCH PREVIEW.png\n"; return 2; }
 Gdiplus::GdiplusStartupInput input; ULONG_PTR token; Gdiplus::GdiplusStartup(&token,&input,nullptr);
 int exit=0;
 {
 auto fonts=ctrport::launcher_font::load({std::filesystem::path(argv[2])/"archives/update.pak",std::filesystem::path(argv[1])/"archives/permanent.pak"});
 std::cout<<fonts.diagnostics;
 if(!fonts.title || !fonts.body) return 3;
 auto base=ctrport::launcher_font::load({std::filesystem::path(argv[1])/"archives/permanent.pak"});
 if(!base.title || !base.body || base.title->glyphs.size()!=291 || fonts.title->glyphs.size()!=294) return 4;
 Gdiplus::Bitmap preview(1040,640,PixelFormat32bppARGB); Gdiplus::Graphics gr(&preview);gr.Clear(Gdiplus::Color(255,19,25,39));
 fonts.title->draw(gr,L"CRASH TEAM RACING",40,{30,20,970,60},0xffffcb55,Gdiplus::StringAlignmentNear);
 fonts.title->draw(gr,L"LOBBY",22,{30,100,180,30},0xffffcb55,Gdiplus::StringAlignmentNear);
 fonts.body->draw(gr,L"COPIA INVITO COMPLETO",12,{250,100,290,32},0xff55efff,Gdiplus::StringAlignmentCenter);
 fonts.body->draw(gr,L"HostTest   8 partecipanti",13,{30,150,400,25},0xffffffff,Gdiplus::StringAlignmentNear);
 fonts.body->draw(gr,L"Nel gioco: Online > Matchmaking\n> Go to Lobby. Restate nella stessa stanza.",10,{30,190,286,34},0xffc4d0dd,Gdiplus::StringAlignmentNear);
 if(!fonts.title->draw(gr,L"ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n0123456789",26,{30,250,950,160},0xffffffff,Gdiplus::StringAlignmentNear)) return 10;
 fonts.body->draw(gr,L"Un nome molto lungo che deve essere troncato",14,{30,450,200,25},0xff55efff,Gdiplus::StringAlignmentNear);
 if(fonts.body->draw(gr,L"\u4e2d",14,{30,500,200,25},0xffffffff,Gdiplus::StringAlignmentNear)) return 5;
 CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0,0,0xf8,0x1e,0xf3,0x2e}};
 if(preview.Save(argv[3],&png)!=Gdiplus::Ok)return 6;
 try { ctrport::launcher_font::decode({1,2,3}); return 7; } catch(const std::exception&) {}
 // Corrupt real font headers/pointers must reject, never read beyond the asset.
 ctrport::xbox_prepare::Archive archive(std::filesystem::path(argv[1])/"archives/permanent.pak",true);
 auto entry=std::find_if(archive.files.begin(),archive.files.end(),[](const auto& e){return e.path=="fonts/zoinks.igz";});
 if(entry==archive.files.end()) return 8;
 const auto original=archive.extract(*entry);
 for(size_t offset : {size_t(0),size_t(4),size_t(24),size_t(28)}) {
     auto damaged=original; ctrport::xbox_prepare::wr32(damaged.data()+offset,0xffffffff);
     try { ctrport::launcher_font::decode(damaged); return 9; } catch(const std::exception&) {}
 }
 auto missing=ctrport::launcher_font::load({});
 if(missing.title || missing.body || missing.diagnostics.empty()) return 11;
 if(fonts.title->draw(gr,L"\u0627",14,{30,500,200,25},0xffffffff,Gdiplus::StringAlignmentNear)) return 12;
 std::cout<<"PASS base + patch counts, Unicode/RTL fallback, missing fonts, truncated/corrupt input, styled text preview\n";
 }
 Gdiplus::GdiplusShutdown(token);return exit;
}
