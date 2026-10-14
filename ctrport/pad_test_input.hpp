// SPDX-License-Identifier: GPL-2.0-only
// Opt-in automation overlay on a genuinely connected XInput controller.
// runtime/user/input/pad.txt: decimal XInput button mask and GetTickCount64 deadline.
// The guest continues to run its original input/menu logic. Never changes connection
// status, never affects an unconfigured process, and expired commands release buttons.
#pragma once
#include <fstream>
namespace ctrport {
inline void apply_pad_test_input(XINPUT_GAMEPAD& gamepad) {
    static const bool enabled=[] {char value[8]{};return GetEnvironmentVariableA("CTR_PAD_TEST_INPUT",value,sizeof(value))==1 && value[0]=='1';}();
    if(!enabled)return;
    unsigned mask=0;unsigned long long deadline=0;
    std::ifstream command("runtime/user/input/pad.txt");
    const auto now=GetTickCount64();
    if(!(command>>mask>>deadline) || mask>65535 || now>deadline || deadline-now>2000)mask=0;
    static unsigned previous=0;
    if(mask!=previous){std::printf("{\"scope\":\"diagnostic_pad_input\",\"xinput_buttons\":%u,\"physical_connection_required\":true}\n",mask);std::fflush(stdout);previous=mask;}
    gamepad.wButtons=static_cast<WORD>(gamepad.wButtons|mask);
}
}
