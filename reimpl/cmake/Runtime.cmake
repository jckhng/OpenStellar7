add_library(stellar7_core
    src/native_frame_pacing.cpp
    src/audio.cpp
    src/audio_clock.cpp
    src/audio_work.cpp
    src/audio_program_work.cpp
    src/enemy20_audio_work.cpp
    src/bitmap_cache.cpp
    src/blackout_work.cpp
    src/cockpit_state.cpp
    src/campaign_loader.cpp
    src/campaign_entity_pools.cpp
    src/campaign_enemy_projectiles.cpp
    src/campaign_presentation.cpp
    src/campaign_frame_boundary.cpp
    src/campaign_guardian.cpp
    src/campaign_guardian_children.cpp
    src/playable_campaign_loader.cpp
    src/playable_campaign_lifecycle.cpp
    src/playable_level1_adoption.cpp
    src/completion_beam.cpp
    src/ending_scene.cpp
    src/story_scene.cpp
    src/playable_story.cpp
    src/native_playable_story.cpp
    src/native_playable_preferences.cpp
    src/native_startup_story.cpp
    src/playable_joystick.cpp
    src/playable_joystick_calibration.cpp
    src/native_joystick.cpp
    src/native_joystick_calibration_run.cpp
    src/native_joystick_calibration_adapters.cpp
    src/playable_attract_controller.cpp
    src/playable_attract.cpp
    src/playable_attract_presentation.cpp
    src/playable_attract_briefing.cpp
    src/playable_attract_intro.cpp
    src/playable_attract_credits.cpp
    src/failure_control_work.cpp
    src/failure_effect_work.cpp
    src/failure_gameplay_work.cpp
    src/failure_heap_work.cpp
    src/continue_work.cpp
    src/continue_widget_work.cpp
    src/continue_vertical_bitmap_work.cpp
    src/continue_gameplay_work.cpp
    src/native_continue_session.cpp
    src/native_retry_session.cpp
    src/retry_work.cpp
    src/frame_copy_work.cpp
    src/frame_controller.cpp
    src/frame_tail_work.cpp
    src/gameplay.cpp
    src/gameplay_renderer.cpp
    src/gameplay_render_support.cpp
    src/world_presentation.cpp
    src/spatial_scene.cpp
    src/spatial_view.cpp
    src/chase_view.cpp
    src/destruction_view.cpp
    src/playable_jump.cpp
    src/playable_decoy.cpp
    src/bomb7_audio_work.cpp
    src/playable_bomb.cpp
    src/ordinary_behavior.cpp
    src/level5_guardian.cpp
    src/highscore.cpp
    src/highscore_render.cpp
    src/input_movie.cpp
    src/indexed_pages.cpp
    src/indexed_render.cpp
    src/keyboard_transport.cpp
    src/keyboard_clock_access.cpp
    src/level1.cpp
    src/level1_trace.cpp
    src/level2_frame.cpp
    src/level2_render_identity.cpp
    src/level2_primary_model.cpp
    src/level2_projectile_model.cpp
    src/level2_combat_model.cpp
    src/level2_combat_tail_model.cpp
    src/level2_periodic_model.cpp
    src/level1_frame.cpp
    src/level1_gameplay_frame.cpp
    src/menu.cpp
    src/menu_model.cpp
    src/native_checkpoint.cpp
    src/native_session.cpp
    src/native_level2_transition_session.cpp
    src/native_level2_frame_session.cpp
    src/native_playable_campaign.cpp
    src/native_playable_lifecycle.cpp
    src/native_playable_title.cpp
    src/native_playable_ending.cpp
    src/native_level2_retry_session.cpp
    src/native_menu.cpp
    src/playable_menu_pages.cpp
    src/playable_menu_dialog.cpp
    src/playable_preferences.cpp
    src/playable_pointer.cpp
    src/briefing_pages.cpp
    src/briefing_preview.cpp
    src/briefing_strip.cpp
    src/native_startup_playable_menu.cpp
    src/native_blackout.cpp
    src/recovered_frontend.cpp
    src/native_startup.cpp
    src/native_startup_presentation.cpp
    src/opl2.cpp
    src/palette_clock.cpp
    src/presentation_clock.cpp
    src/render_work.cpp
    src/retained_frame.cpp
    src/retained_level2.cpp
    src/controlled_session.cpp
    src/controlled_keyboard.cpp
    src/presentation_transport.cpp
    src/polygon_work.cpp
    src/raster_line_work.cpp
    src/raster_point_work.cpp
    src/radar_work.cpp
    src/radar_state.cpp
    src/sight_work.cpp
    src/simulation_prefix_work.cpp
    src/visibility_state.cpp
    src/resource_codec.cpp
    src/scene_script.cpp
    src/sampled_voice.cpp
    src/sound_blaster.cpp
    src/success_audio_work.cpp
    src/success_control_work.cpp
    src/success_initial_work.cpp
    src/timer_work.cpp
    src/timer_install_work.cpp
    src/timer_registration_work.cpp
    src/startup_context_work.cpp
    src/startup_settings_work.cpp
    src/driver_initialization_work.cpp
    src/startup_resources_work.cpp
    src/startup_presentation_work.cpp
    src/startup_fonts_work.cpp
    src/native_startup_fonts.cpp
    src/startup_logo_work.cpp
    src/native_startup_logo.cpp
    src/native_startup_logo_end.cpp
    src/startup_intro_work.cpp
    src/native_startup_intro.cpp
    src/native_startup_menu_song.cpp
    src/native_startup_menu_resources.cpp
    src/native_startup_credits.cpp
    src/native_startup_menu_entry.cpp
    src/native_startup_begin.cpp
    src/native_startup_adoption.cpp
    src/level1_loader_work.cpp
    src/level1_primary_fire_work.cpp
    src/level1_periodic_spawn_work.cpp
    src/level2_periodic_spawn_work.cpp
    src/target_hit_work.cpp
    src/level1_arrival_work.cpp
    src/level1_type7_work.cpp
    src/guardian_deployment_work.cpp
    src/level1_deployment_render_work.cpp
    src/type42_bonus_work.cpp
    src/type42_pickup_work.cpp
    src/score_work.cpp
    src/weapon_lamp_work.cpp
    src/native_level1_session.cpp
    src/guardian_reward_work.cpp
    src/level1_success_model.cpp
    src/level1_success_resume_model.cpp
    src/recovered_level2_model.cpp
    src/recovered_level2_retry_model.cpp
    src/level1_transition_work.cpp
    src/level2_loader_pages_work.cpp
    src/level2_music_work.cpp
    src/native_success_session.cpp
    src/native_failure_session.cpp
    src/menu_widget_work.cpp
    src/menu_drawing_work.cpp
    src/ui_pointer_work.cpp
    src/credits_work.cpp
    src/ui_bitmap_work.cpp
    src/ui_panel_work.cpp
    src/ui_fill_work.cpp
    src/ui_text_work.cpp
    src/driver_sequence_work.cpp
    src/world_traversal.cpp
    src/world_model_work.cpp
    src/world_scene_work.cpp
    src/level1_world_work.cpp
    third_party/ymfm/ymfm_adpcm.cpp
    third_party/ymfm/ymfm_opl.cpp
    third_party/ymfm/ymfm_pcm.cpp
)
target_include_directories(stellar7_core
    PUBLIC include
    PRIVATE third_party/ymfm
)

option(STELLAR7_ENHANCED "Build the SDL2 enhanced presentation and controls" OFF)
find_package(SDL2 2.0.10 CONFIG QUIET)
if(SDL2_FOUND AND TARGET SDL2::SDL2)
    set(stellar7_sdl_library SDL2::SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(STELLAR7_SDL2 REQUIRED IMPORTED_TARGET "sdl2>=2.0.10")
    set(stellar7_sdl_library PkgConfig::STELLAR7_SDL2)
endif()
add_library(stellar7_sdl INTERFACE)
target_link_libraries(stellar7_sdl INTERFACE ${stellar7_sdl_library})
add_library(stellar7_player src/native_profile.cpp src/native_install.cpp src/native_player.cpp src/native_player_sdl.cpp)
target_link_libraries(stellar7_player PUBLIC stellar7_core PRIVATE stellar7_sdl)

add_library(stellar7_assets src/gameplay_asset_loader.cpp src/resource_file.cpp)
target_link_libraries(stellar7_assets PUBLIC stellar7_core)

add_library(stellar7_host src/native_player_entry.cpp src/frontend_sdl.cpp)
target_link_libraries(stellar7_host PUBLIC stellar7_player stellar7_assets stellar7_sdl)

add_executable(stellar7 src/main.cpp)
target_link_libraries(stellar7 PRIVATE stellar7_host)
set(stellar7_executables stellar7)
option(STELLAR7_BUILD_DIAGNOSTICS "Build the separate developer trace/replay executable" OFF)
if(STELLAR7_BUILD_DIAGNOSTICS)
    add_executable(stellar7-tools
        src/diagnostic_main.cpp
        src/diagnostic_sound_blaster_work.cpp)
    target_link_libraries(stellar7-tools PRIVATE stellar7_host)
    list(APPEND stellar7_executables stellar7-tools)
endif()
if(STELLAR7_ENHANCED)
    find_package(Threads REQUIRED)
    add_library(stellar7_gl src/gl_scene_renderer.cpp src/xr_view.cpp src/xr_presentation.cpp)
    target_include_directories(stellar7_gl PUBLIC src)
    target_link_libraries(stellar7_gl PUBLIC stellar7_core PRIVATE stellar7_sdl)
    target_sources(stellar7_host PRIVATE src/gl_sdl_presenter.cpp)
    target_link_libraries(stellar7_host PRIVATE stellar7_gl)
    option(STELLAR7_OPENXR "Build the optional OpenXR runtime adapter (SDK headers required)" OFF)
    if(STELLAR7_OPENXR)
        find_path(STELLAR7_OPENXR_INCLUDE_DIR openxr/openxr.h REQUIRED)
        add_library(stellar7_openxr src/openxr_session.cpp src/openxr_actions.cpp src/openxr_loader.cpp src/openxr_native.cpp src/openxr_pump.cpp)
        target_link_libraries(stellar7_openxr PUBLIC stellar7_gl PRIVATE stellar7_sdl)
        target_include_directories(stellar7_openxr SYSTEM PUBLIC "${STELLAR7_OPENXR_INCLUDE_DIR}")
        add_library(stellar7_openxr_host src/openxr_sdl.cpp)
        target_link_libraries(stellar7_openxr_host PUBLIC stellar7_openxr PRIVATE stellar7_sdl Threads::Threads)
        target_link_libraries(stellar7_host PRIVATE stellar7_openxr_host)
        target_compile_definitions(stellar7_host PRIVATE STELLAR7_OPENXR_HOST=1)
    endif()
    option(STELLAR7_GL_HEADLESS_TESTS "Validate GL/GLES through software-only surfaceless EGL" OFF)
    target_link_libraries(stellar7_host PRIVATE Threads::Threads)
    target_sources(stellar7_player PRIVATE src/enhanced_options.cpp)
    target_sources(stellar7_player PRIVATE src/destruction_audio.cpp)
    target_sources(stellar7_host PRIVATE src/enhanced_sdl.cpp)
    target_compile_definitions(stellar7_player PRIVATE STELLAR7_ENHANCED=1)
    target_compile_definitions(stellar7_host PRIVATE STELLAR7_ENHANCED=1)
endif()
if(WIN32)
    target_sources(stellar7_host PRIVATE src/frontend_win32.cpp src/controlled_frontend_win32.cpp)
    target_link_libraries(stellar7_host PRIVATE user32 gdi32 winmm)
else()
    target_sources(stellar7_host PRIVATE src/frontend_terminal.cpp src/controlled_frontend_unsupported.cpp)
endif()
foreach(executable IN LISTS stellar7_executables)
    # Preserve the existing startup/diagnostic stack allowance for both entries.
    target_link_options(${executable} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:4194304>)
    if(WIN32)
        target_link_libraries(${executable} PRIVATE SDL2::SDL2main)
        add_custom_command(TARGET ${executable} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${stellar7_sdl_library}> $<TARGET_FILE_DIR:${executable}>)
    endif()
endforeach()
if(WIN32 AND STELLAR7_ENHANCED)
    add_custom_command(TARGET stellar7 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/windows/Play-safe.cmd" "$<TARGET_FILE_DIR:stellar7>/Play.cmd"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/windows/Play-safe.cmd" "$<TARGET_FILE_DIR:stellar7>/Play-safe.cmd"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/windows/Play-gamepad.cmd" "$<TARGET_FILE_DIR:stellar7>/Play-gamepad.cmd")
endif()
