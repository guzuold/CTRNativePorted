// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace Frontend {
struct ExitGesture {
    enum class Action { None, Open, Cancel, Confirm };
    bool visible=false, captured=false, ready=false, holding=false, draining=false;
    std::uint64_t since=0;
    Action update(std::uint16_t buttons,bool connected,bool focused,std::uint64_t now){
        constexpr unsigned chord=0x30, a=0x1000, b=0x2000;
        if(!connected || !focused){bool was=visible;*this={};return was?Action::Cancel:Action::None;}
        if(draining){captured=buttons!=0;if(!buttons)draining=false;return Action::None;}
        if(visible){
            captured=true;
            if(!buttons)ready=true;
            if(ready && (buttons&(a|b))){visible=false;ready=false;draining=true;return (buttons&b)?Action::Cancel:Action::Confirm;}
            return Action::None;
        }
        if((buttons&chord)==chord){
            captured=true;
            if(!holding){holding=true;since=now;}
            if(now-since>=1200){visible=true;ready=false;holding=false;return Action::Open;}
        }else{holding=false;captured=false;}
        return Action::None;
    }
    void cancel(){visible=false;ready=false;holding=false;draining=true;captured=true;}
};
}
