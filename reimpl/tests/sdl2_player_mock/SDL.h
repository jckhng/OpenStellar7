#pragma once
#include <cstdint>
using Uint32=std::uint32_t;
struct SDL_Window {};
constexpr Uint32 SDL_MESSAGEBOX_ERROR=0x10;
const char* SDL_GetError();
char* SDL_getenv(const char*);
char* SDL_GetPrefPath(const char*,const char*);
char* SDL_GetBasePath();
void SDL_free(void*);
int SDL_ShowSimpleMessageBox(Uint32,const char*,const char*,SDL_Window*);
