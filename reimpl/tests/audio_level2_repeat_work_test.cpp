// Reuse the twelve original-qualified FC/restart/successor snapshots unchanged.
#include "stellar7/audio_repeat_work.hpp"
#define soundBlasterLevel1CallbackAdvance soundBlasterLevel2CallbackAdvance
#define main included_level2_repeat_controls_main
#include "audio_repeat_work_test.cpp"
#undef main
#undef soundBlasterLevel1CallbackAdvance
int main() {return included_level2_repeat_controls_main();}
