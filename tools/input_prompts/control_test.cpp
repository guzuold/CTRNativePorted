// SPDX-License-Identifier: GPL-2.0-only
#include "../../ctrport/gpu/shim/controller_exit_state.h"
#include "../../ctrport/app0.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
    using G=Frontend::ExitGesture;using A=G::Action;
    G g;
    assert(g.update(0x30,true,true,0)==A::None && g.captured);
    assert(g.update(0x30,true,true,1199)==A::None);
    assert(g.update(0x30,true,true,1200)==A::Open);
    assert(g.update(0x1000,true,true,1210)==A::None); // Must release first.
    assert(g.update(0,true,true,1220)==A::None);
    assert(g.update(0x2000,true,true,1230)==A::Cancel);
    assert(g.update(0x30,true,true,3000)==A::None); // Drain after cancel.
    g.update(0,true,true,3001);g.update(0x30,true,true,3002);
    assert(g.update(0x30,true,true,4202)==A::Open);
    g.update(0,true,true,4203);
    assert(g.update(0x1000,true,true,4204)==A::Confirm);
    g={};g.update(0x30,true,true,1);
    assert(g.update(0x30,true,false,2000)==A::None && !g.captured);
    g.update(0x30,true,true,2001);g.update(0x30,true,true,3201);
    assert(g.update(0,false,true,3202)==A::Cancel && !g.captured);
    if(argc>=2)for(const char* archive : {"archives/permanent.pak","archives/languagestartup.pak"}){
        _putenv_s("CTR_XBOX_PROMPTS","0");
        auto original=ctrport::app0_read_path(argv[1],{},archive);
        _putenv_s("CTR_XBOX_PROMPTS","1");
        auto modified=ctrport::app0_read_path(argv[1],{},archive);
        assert(original!=modified && std::filesystem::is_regular_file(modified));
        _putenv_s("CTR_XBOX_PROMPTS","0");
        assert(ctrport::app0_read_path(argv[1],{},archive)==original);
    }
    if(argc==3){
        _putenv_s("CTR_XBOX_PROMPTS","0");
        const auto original=ctrport::app0_read_path(argv[1],argv[2],"archives/update.pak");
        _putenv_s("CTR_XBOX_PROMPTS","1");
        const auto modified=ctrport::app0_read_path(argv[1],argv[2],"archives/update.pak");
        assert(original!=modified && std::filesystem::is_regular_file(modified));
        _putenv_s("CTR_XBOX_PROMPTS","0");
        assert(ctrport::app0_read_path(argv[1],argv[2],"archives/update.pak")==original);
    }
    std::cout<<"PASS: exit hold/release/cancel/confirm/focus/disconnect; retail overlay selection and PS4 fallback\n";
}
