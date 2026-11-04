# Rights boundary

OpenStellar7 is an unofficial reimplementation. Its MIT grant applies to
project-authored source and documentation. The retained dos-re-harness copyright
notice acknowledges the code's development origin.

The grant does not include the original Stellar 7 executable, data, level/model
resources, artwork, music, voices, trademarks, captures or saves. Obtain original
data separately and decode it locally. Do not commit imported data or private
evidence. A source licence does not grant rights to redistribute original assets.

Third-party components retain their own licences. This source tree includes ymfm
under BSD-3-Clause. SDL and optional OpenXR dependencies are obtained separately;
preserve their applicable notices when distributing binaries. No emulator or
decompiler is bundled or required to run the player.

## Third-party components

| Component | Source/distribution | Licence and location |
| --- | --- | --- |
| ymfm, Aaron Giles | Vendored OPL/FM implementation | BSD-3-Clause, `reimpl/third_party/ymfm/LICENSE` and file headers |
| SDL2 | External development/runtime dependency | zlib; runtime notice in `tools/package/payload/notices/SDL2-LICENSE.txt` |
| OpenXR SDK/loader | Optional external XR dependency | Use the official Khronos distribution's applicable licences/notices; no SDK or loader is bundled in this source tree |

Project-authored code uses MIT; the earlier dos-re-harness copyright remains
alongside OpenStellar7 contributors in LICENSE. Original game resources are not
licensed by this source distribution. 
