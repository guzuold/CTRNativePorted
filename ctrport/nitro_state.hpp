// SPDX-License-Identifier: GPL-2.0-only
// Presentation state is independent of the retail/game configuration.
bool nitro_motion = true;
HWND nitro_overlay = nullptr;
HBITMAP nitro_from = nullptr, nitro_to = nullptr;
HBITMAP nitro_frame = nullptr;
HDC nitro_from_dc = nullptr, nitro_to_dc = nullptr, nitro_frame_dc = nullptr;
unsigned nitro_frames=0;
ULONGLONG nitro_last_frame=0,nitro_max_gap=0,nitro_capture_ms=0;
ULONGLONG nitro_started = 0;
float nitro_nav_from = 0, nitro_nav_to = 0;
bool nitro_animating = false;
bool nitro_preparing = false;
bool nitro_finishing = false;
std::vector<HWND> nitro_hidden_controls;
HBITMAP nitro_bases[2] = {};
HBITMAP nitro_page_background = nullptr;
int nitro_page_background_tab = -1;
constexpr UINT_PTR kNitroTimer = 502;
constexpr int ID_NITRO_FRIENDS = 907, ID_NITRO_MOTION = 908;
void nitro_begin(int tab);
void nitro_finish(bool repaint=true);
void nitro_tick();

float nitro_progress() { return nitro_preparing?0.f:nitro_animating ? nitro::ease(float(GetTickCount64()-nitro_started)/nitro::duration_ms) : 1.f; }
