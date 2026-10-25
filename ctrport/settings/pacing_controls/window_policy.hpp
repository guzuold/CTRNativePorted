// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace ctrport::settings::pacing {
inline const char* setting(const char* key){const char* p=std::getenv(key);return p?p:"original";}
inline void require_sdl(bool result,const char* message){if(!result)throw std::runtime_error(message);}
inline void apply_window(SDL_Window* window,int width,int height){
 const auto mode=setting("CTR_SETTINGS_WINDOW_MODE"),refresh=setting("CTR_SETTINGS_DISPLAY_REFRESH");
 const bool original=!std::strcmp(mode,"original"),exclusive=!std::strcmp(mode,"exclusive"),borderless=!std::strcmp(mode,"borderless"),windowed=!std::strcmp(mode,"windowed");
 if(!original&&!exclusive&&!borderless&&!windowed)throw std::invalid_argument("invalid window mode");
 if(!exclusive&&std::strcmp(refresh,"original"))throw std::invalid_argument("display refresh requires exclusive fullscreen");
 if(original)return;
 const auto display=SDL_GetDisplayForWindow(window);require_sdl(display!=0,"display lookup failed");
 SDL_DisplayMode chosen{};
 if(exclusive){
  float hz=0;
  if(!std::strcmp(refresh,"original")){auto desktop=SDL_GetCurrentDisplayMode(display);require_sdl(desktop!=nullptr,"desktop mode unavailable");hz=desktop->refresh_rate;}
  else {for(const char* allowed:{"60","90","120","144","165","240"})if(!std::strcmp(refresh,allowed))hz=std::strtof(refresh,nullptr);if(hz==0)throw std::invalid_argument("invalid display refresh");}
  int count=0;auto modes=SDL_GetFullscreenDisplayModes(display,&count);require_sdl(modes!=nullptr,"fullscreen mode query failed");
  bool found=false;for(int i=0;i<count;++i)if(modes[i]->w==width&&modes[i]->h==height&&std::abs(modes[i]->refresh_rate-hz)<=0.1f){chosen=*modes[i];found=true;break;}
  SDL_free(modes);require_sdl(found,"requested resolution/refresh is unavailable on this display");
 }
 require_sdl(SDL_SetWindowFullscreenMode(window,exclusive?&chosen:nullptr),"fullscreen mode selection failed");
 require_sdl(SDL_SetWindowFullscreen(window,!windowed),"fullscreen transition failed");
 require_sdl(SDL_SyncWindow(window),"fullscreen transition did not settle");
 const bool active=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN)!=0;
 require_sdl(active==!windowed,"window mode was not acknowledged");
 const auto selected=SDL_GetWindowFullscreenMode(window);
 require_sdl(exclusive?selected!=nullptr:selected==nullptr,"exclusive mode was not acknowledged");
 const auto actual=SDL_GetCurrentDisplayMode(display);require_sdl(actual!=nullptr,"active display mode unavailable");
 if(exclusive)require_sdl(actual->w==chosen.w&&actual->h==chosen.h&&std::abs(actual->refresh_rate-chosen.refresh_rate)<=0.1f,"display did not acknowledge requested refresh/resolution");
 std::printf("CTR_SETTINGS_WINDOW requested=%s applied=%s refresh_hz=%.3f\n",mode,mode,actual->refresh_rate);
}
}
