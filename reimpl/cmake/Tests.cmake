include(cmake/EnhancedTests.cmake)

set(player_cli_boundary_args "-DPLAYER=$<TARGET_FILE:stellar7>")
if(TARGET stellar7-tools)
    list(APPEND player_cli_boundary_args "-DTOOLS=$<TARGET_FILE:stellar7-tools>")
endif()
add_test(NAME stellar7_player_cli_boundary_tests COMMAND "${CMAKE_COMMAND}" ${player_cli_boundary_args}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/player_cli_boundary_test.cmake")
foreach(player_test native_profile native_install)
    add_executable(stellar7_${player_test}_tests tests/${player_test}_test.cpp tests/assertions_enabled.cpp)
    target_include_directories(stellar7_${player_test}_tests PRIVATE src)
    target_link_libraries(stellar7_${player_test}_tests PRIVATE stellar7_player)
    target_compile_options(stellar7_${player_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${player_test}_tests COMMAND stellar7_${player_test}_tests
        "${CMAKE_CURRENT_BINARY_DIR}/test-output/player-install")
endforeach()



add_executable(stellar7_native_player_sdl2_tests tests/native_player_sdl2_test.cpp src/native_player_sdl.cpp)
target_include_directories(stellar7_native_player_sdl2_tests PRIVATE tests/sdl2_player_mock include src)
add_test(NAME stellar7_native_player_sdl2_tests COMMAND stellar7_native_player_sdl2_tests)
add_executable(stellar7_native_player_launch_tests
    tests/native_player_launch_test.cpp src/native_player.cpp src/native_profile.cpp)
target_include_directories(stellar7_native_player_launch_tests PRIVATE include src)
target_link_libraries(stellar7_native_player_launch_tests PRIVATE stellar7_core)
add_test(NAME stellar7_native_player_launch_tests COMMAND stellar7_native_player_launch_tests
    "${CMAKE_CURRENT_BINARY_DIR}/test-output/datafile-launch")
add_executable(stellar7_success_initial_work_tests tests/success_initial_work_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_success_initial_work_tests PRIVATE stellar7_core)
target_compile_options(stellar7_success_initial_work_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_success_initial_work_tests COMMAND stellar7_success_initial_work_tests)
foreach(continue_test level2_retry_model level1_unbanked_lift_work level2_precision_render_work level2_horizon_work level1_type20_work level2_primary_allocation_work level2_primary_model level2_hit_audio enemy20_audio level2_constructor_radar_work level2_control_work level1_axis_tank_work recovered_level2_effect_reset positive_score_retry audio_repeat_work guardian_deployment_work guardian_projectile_work guardian_nonlethal_work guardian_primary_reuse_work guardian_tail_work level1_deployment_render_work success_audio_work success_control_work level1_arrival_work level1_arrival_tail_work level1_arrival_prefix_work level1_arrival_render_work gameplay_arrival_event success_palette_clock level1_type42_bonus_work type42_pickup_work bonus_target_hit_work bonus_prefix_work bonus_pickup_prefix_work level1_bonus_tail_membership level1_bonus_render_work gameplay_bonus_pickup_event initial_target_hit_work initial_target_score_work initial_target_prefix_work level1_retired_lift_frame level1_target_prefix_work combat_scene_work level1_combat_effect_tail level1_target_gameplay_work gameplay_target_hit_event target_hit_work score_work combat_effect_work world_polygon_work continue_work continue_gameplay_work retry_work level1_retry_music_work recovered_retry_boundary level1_periodic_spawn_work level1_periodic_fire_tail level1_dynamic_membership level1_dynamic_radar level1_dynamic_frame level1_periodic_gameplay_work)
    add_executable(stellar7_${continue_test}_tests tests/${continue_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${continue_test}_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_${continue_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${continue_test}_tests COMMAND stellar7_${continue_test}_tests)
endforeach()
add_executable(stellar7_owned_collapsed_polygon_tests tests/owned_collapsed_polygon_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_owned_collapsed_polygon_tests PRIVATE stellar7_core)
target_compile_options(stellar7_owned_collapsed_polygon_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_owned_collapsed_polygon_tests COMMAND stellar7_owned_collapsed_polygon_tests)
foreach(guardian_test level5_guardian_behavior level5_guardian_entry level5_guardian_defeat scene_script sampled_voice success_return ending_entry native_irq_wait native_blackout menu_audio_return native_menu menu_begin_model level1_failure_tail level1_success_contact success_palette_irq level2_success_return level3_loader timer_registration)
    add_executable(stellar7_${guardian_test}_tests tests/${guardian_test}_test.cpp tests/assertions_enabled.cpp)
    target_include_directories(stellar7_${guardian_test}_tests PRIVATE src)
    target_link_libraries(stellar7_${guardian_test}_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_${guardian_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${guardian_test}_tests COMMAND stellar7_${guardian_test}_tests)
endforeach()
add_executable(stellar7_highscore_tests tests/highscore_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_highscore_tests PRIVATE stellar7_core)
target_compile_options(stellar7_highscore_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_highscore_tests COMMAND stellar7_highscore_tests "${CMAKE_CURRENT_BINARY_DIR}/test-output/highscore")
add_executable(stellar7_highscore_render_tests tests/highscore_render_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_highscore_render_tests PRIVATE stellar7_core)
target_compile_options(stellar7_highscore_render_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_highscore_render_tests COMMAND stellar7_highscore_render_tests)
add_executable(stellar7_native_session_tests tests/native_session_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_native_session_tests PRIVATE stellar7_core)
target_compile_options(stellar7_native_session_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_native_session_tests COMMAND stellar7_native_session_tests)
add_executable(stellar7_host_keyboard_tests tests/host_keyboard_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_host_keyboard_tests PRIVATE stellar7_core)
target_compile_options(stellar7_host_keyboard_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_host_keyboard_tests COMMAND stellar7_host_keyboard_tests)
add_executable(stellar7_keyboard_irq_tests tests/keyboard_irq_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_keyboard_irq_tests PRIVATE stellar7_core)
target_include_directories(stellar7_keyboard_irq_tests PRIVATE src)
target_compile_options(stellar7_keyboard_irq_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_keyboard_irq_tests COMMAND stellar7_keyboard_irq_tests)

add_executable(stellar7_keyboard_transport_tests tests/keyboard_transport_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_keyboard_transport_tests PRIVATE stellar7_core)
target_compile_options(stellar7_keyboard_transport_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_keyboard_transport_tests COMMAND stellar7_keyboard_transport_tests)

add_executable(stellar7_forward_tail_tests tests/forward_tail_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_forward_tail_tests PRIVATE stellar7_core)
target_compile_options(stellar7_forward_tail_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_forward_tail_tests COMMAND stellar7_forward_tail_tests)

add_executable(stellar7_audio_timeline_tests tests/audio_timeline_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_audio_timeline_tests PRIVATE stellar7_core)
target_compile_options(stellar7_audio_timeline_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_audio_timeline_tests COMMAND stellar7_audio_timeline_tests)

add_executable(stellar7_presentation_transport_tests tests/presentation_transport_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_presentation_transport_tests PRIVATE stellar7_core)
target_compile_options(stellar7_presentation_transport_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_presentation_transport_tests COMMAND stellar7_presentation_transport_tests)

add_executable(stellar7_controlled_session_tests tests/controlled_session_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_controlled_session_tests PRIVATE stellar7_core)
target_compile_options(stellar7_controlled_session_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_controlled_session_tests COMMAND stellar7_controlled_session_tests)

add_executable(stellar7_scanout_delivery_tests tests/scanout_delivery_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_scanout_delivery_tests PRIVATE stellar7_core)
target_compile_options(stellar7_scanout_delivery_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_scanout_delivery_tests COMMAND stellar7_scanout_delivery_tests)

add_executable(stellar7_retained_level2_tests tests/retained_level2_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_retained_level2_tests PRIVATE stellar7_core)
target_compile_options(stellar7_retained_level2_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_retained_level2_tests COMMAND stellar7_retained_level2_tests)

add_executable(stellar7_retained_frame_tests tests/retained_frame_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_retained_frame_tests PRIVATE stellar7_core)
target_compile_options(stellar7_retained_frame_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_retained_frame_tests COMMAND stellar7_retained_frame_tests)

add_executable(stellar7_core_tests
    tests/gameplay_test.cpp
    tests/level1_test.cpp
    tests/menu_model_test.cpp
    tests/resource_codec_test.cpp
)
target_link_libraries(stellar7_core_tests PRIVATE stellar7_core)
# The combined gameplay test retains many independent model fixtures on its stack.
target_link_options(stellar7_core_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:4194304>)
add_test(NAME stellar7_core_tests COMMAND stellar7_core_tests)

add_executable(stellar7_ordinary_behavior_tests tests/ordinary_behavior_test.cpp tests/assertions_enabled.cpp)
target_include_directories(stellar7_ordinary_behavior_tests PRIVATE src)
target_link_libraries(stellar7_ordinary_behavior_tests PRIVATE stellar7_core)
target_compile_options(stellar7_ordinary_behavior_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_ordinary_behavior_tests COMMAND stellar7_ordinary_behavior_tests)

add_executable(stellar7_ordinary_bonus_tests tests/ordinary_bonus_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_ordinary_bonus_tests PRIVATE stellar7_core)
target_compile_options(stellar7_ordinary_bonus_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_ordinary_bonus_tests COMMAND stellar7_ordinary_bonus_tests)

add_executable(stellar7_fixed_reuse_tests tests/fixed_reuse_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_fixed_reuse_tests PRIVATE stellar7_core)
target_compile_options(stellar7_fixed_reuse_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_fixed_reuse_tests COMMAND stellar7_fixed_reuse_tests)

add_executable(stellar7_completion_entry_tests tests/completion_entry_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_completion_entry_tests PRIVATE stellar7_core)
target_compile_options(stellar7_completion_entry_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_completion_entry_tests COMMAND stellar7_completion_entry_tests)

add_executable(stellar7_audio_tests tests/audio_test.cpp)
target_link_libraries(stellar7_audio_tests PRIVATE stellar7_core)
add_test(NAME stellar7_audio_tests COMMAND stellar7_audio_tests)

add_executable(stellar7_bios_clock_tests tests/bios_clock_test.cpp)
target_link_libraries(stellar7_bios_clock_tests PRIVATE stellar7_core)
add_test(NAME stellar7_bios_clock_tests COMMAND stellar7_bios_clock_tests)

add_executable(stellar7_rtc_clock_tests tests/rtc_clock_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_rtc_clock_tests PRIVATE stellar7_core)
target_compile_options(stellar7_rtc_clock_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
add_test(NAME stellar7_rtc_clock_tests COMMAND stellar7_rtc_clock_tests)

add_executable(stellar7_bitmap_cache_tests tests/bitmap_cache_test.cpp)
target_link_libraries(stellar7_bitmap_cache_tests PRIVATE stellar7_core)
add_test(NAME stellar7_bitmap_cache_tests COMMAND stellar7_bitmap_cache_tests)

add_executable(stellar7_page_flip_clock_tests tests/page_flip_clock_test.cpp)
target_link_libraries(stellar7_page_flip_clock_tests PRIVATE stellar7_core)
add_test(NAME stellar7_page_flip_clock_tests COMMAND stellar7_page_flip_clock_tests)

add_executable(stellar7_palette_clock_tests tests/palette_clock_test.cpp)
target_link_libraries(stellar7_palette_clock_tests PRIVATE stellar7_core)
add_test(NAME stellar7_palette_clock_tests COMMAND stellar7_palette_clock_tests)

add_executable(stellar7_blackout_work_tests tests/blackout_work_test.cpp)
target_link_libraries(stellar7_blackout_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_blackout_work_tests COMMAND stellar7_blackout_work_tests)
target_compile_options(stellar7_blackout_work_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
target_sources(stellar7_blackout_work_tests PRIVATE tests/assertions_enabled.cpp)

add_executable(stellar7_ground_work_tests tests/ground_work_control.cpp)
target_link_libraries(stellar7_ground_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_ground_work_tests COMMAND stellar7_ground_work_tests)

add_executable(stellar7_campaign_loader_tests tests/campaign_loader_test.cpp)
target_link_libraries(stellar7_campaign_loader_tests PRIVATE stellar7_core)
add_test(NAME stellar7_campaign_loader_tests COMMAND stellar7_campaign_loader_tests)

add_executable(stellar7_playable_level2_guardian_tests
    tests/playable_level2_guardian_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_playable_level2_guardian_tests PRIVATE stellar7_core)
target_compile_options(stellar7_playable_level2_guardian_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_playable_level2_guardian_tests COMMAND stellar7_playable_level2_guardian_tests)

add_executable(stellar7_playable_campaign_audio_tests
    tests/playable_campaign_audio_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_playable_campaign_audio_tests PRIVATE stellar7_core)
target_compile_options(stellar7_playable_campaign_audio_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_playable_campaign_audio_tests COMMAND stellar7_playable_campaign_audio_tests)

add_executable(stellar7_campaign_guardian_tests
    tests/campaign_guardian_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_campaign_guardian_tests PRIVATE stellar7_core)
target_compile_options(stellar7_campaign_guardian_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_campaign_guardian_tests COMMAND stellar7_campaign_guardian_tests)

add_executable(stellar7_campaign_guardian_children_tests
    tests/campaign_guardian_children_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_campaign_guardian_children_tests PRIVATE stellar7_core)
target_compile_options(stellar7_campaign_guardian_children_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_campaign_guardian_children_tests COMMAND stellar7_campaign_guardian_children_tests)

add_executable(stellar7_campaign_allocation_tests tests/campaign_allocation_test.cpp tests/assertions_enabled.cpp)
target_include_directories(stellar7_campaign_allocation_tests PRIVATE src)
target_link_libraries(stellar7_campaign_allocation_tests PRIVATE stellar7_core)
target_compile_options(stellar7_campaign_allocation_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_campaign_allocation_tests COMMAND stellar7_campaign_allocation_tests)

add_executable(stellar7_world_work_tests tests/world_work_control.cpp)
target_link_libraries(stellar7_world_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_world_work_tests COMMAND stellar7_world_work_tests)

add_executable(stellar7_frame_boundary_tests tests/frame_boundary_test.cpp)
target_link_libraries(stellar7_frame_boundary_tests PRIVATE stellar7_core)
add_test(NAME stellar7_frame_boundary_tests COMMAND stellar7_frame_boundary_tests)

add_executable(stellar7_world_model_work_tests tests/world_model_work_test.cpp)
target_link_libraries(stellar7_world_model_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_world_model_work_tests COMMAND stellar7_world_model_work_tests)

add_executable(stellar7_polygon_work_tests tests/polygon_work_test.cpp)
target_link_libraries(stellar7_polygon_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_polygon_work_tests COMMAND stellar7_polygon_work_tests)

add_executable(stellar7_raster_line_work_tests tests/raster_line_work_control.cpp)
target_link_libraries(stellar7_raster_line_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_raster_line_work_tests COMMAND stellar7_raster_line_work_tests)

add_executable(stellar7_world_actor_profile_tests tests/world_actor_profile_test.cpp)
target_link_libraries(stellar7_world_actor_profile_tests PRIVATE stellar7_core)
add_test(NAME stellar7_world_actor_profile_tests COMMAND stellar7_world_actor_profile_tests)

add_executable(stellar7_raster_point_work_tests tests/raster_point_work_control.cpp)
target_link_libraries(stellar7_raster_point_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_raster_point_work_tests COMMAND stellar7_raster_point_work_tests)

add_executable(stellar7_radar_work_tests tests/radar_work_test.cpp)
target_link_libraries(stellar7_radar_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_radar_work_tests COMMAND stellar7_radar_work_tests)

add_executable(stellar7_sight_work_tests tests/sight_work_test.cpp)
target_link_libraries(stellar7_sight_work_tests PRIVATE stellar7_core)
add_test(NAME stellar7_sight_work_tests COMMAND stellar7_sight_work_tests)

add_executable(stellar7_sight_predicate_tests tests/sight_predicate_test.cpp)
target_link_libraries(stellar7_sight_predicate_tests PRIVATE stellar7_core)
add_test(NAME stellar7_sight_predicate_tests COMMAND stellar7_sight_predicate_tests)

add_executable(stellar7_cockpit_state_tests tests/cockpit_state_test.cpp)
target_link_libraries(stellar7_cockpit_state_tests PRIVATE stellar7_core)
add_test(NAME stellar7_cockpit_state_tests COMMAND stellar7_cockpit_state_tests)

add_executable(stellar7_indexed_pages_tests tests/indexed_pages_test.cpp)
target_link_libraries(stellar7_indexed_pages_tests PRIVATE stellar7_core)
add_test(NAME stellar7_indexed_pages_tests COMMAND stellar7_indexed_pages_tests)

add_executable(stellar7_radar_state_tests tests/radar_state_test.cpp)
target_link_libraries(stellar7_radar_state_tests PRIVATE stellar7_core)
add_test(NAME stellar7_radar_state_tests COMMAND stellar7_radar_state_tests)

# Test assertions must remain live when linked against the optimized core.
add_executable(stellar7_quiet_scanout_tests tests/quiet_scanout_test.cpp)
target_link_libraries(stellar7_quiet_scanout_tests PRIVATE stellar7_core)
add_test(NAME stellar7_quiet_scanout_tests COMMAND stellar7_quiet_scanout_tests)
target_compile_options(stellar7_quiet_scanout_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
target_sources(stellar7_quiet_scanout_tests PRIVATE tests/assertions_enabled.cpp)

foreach(work_test frame_copy_work frame_controller frame_tail_work simulation_prefix_work visibility_state queued_tail)
    add_executable(stellar7_${work_test}_tests tests/${work_test}_test.cpp)
    target_link_libraries(stellar7_${work_test}_tests PRIVATE stellar7_core)
    add_test(NAME stellar7_${work_test}_tests COMMAND stellar7_${work_test}_tests)
endforeach()

foreach(test_target stellar7_palette_clock_tests stellar7_core_tests stellar7_audio_tests stellar7_bios_clock_tests stellar7_bitmap_cache_tests stellar7_page_flip_clock_tests stellar7_ground_work_tests stellar7_campaign_loader_tests stellar7_world_work_tests stellar7_frame_boundary_tests stellar7_world_model_work_tests stellar7_polygon_work_tests stellar7_raster_line_work_tests stellar7_world_actor_profile_tests stellar7_raster_point_work_tests stellar7_radar_work_tests stellar7_sight_work_tests stellar7_sight_predicate_tests stellar7_cockpit_state_tests stellar7_indexed_pages_tests stellar7_radar_state_tests stellar7_frame_copy_work_tests stellar7_frame_controller_tests stellar7_frame_tail_work_tests stellar7_simulation_prefix_work_tests stellar7_visibility_state_tests stellar7_queued_tail_tests)
    target_compile_options(${test_target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
    )
    target_sources(${test_target} PRIVATE tests/assertions_enabled.cpp)
endforeach()

add_executable(stellar7_moving_prefix_tests tests/moving_prefix_test.cpp)
target_link_libraries(stellar7_moving_prefix_tests PRIVATE stellar7_core)
add_test(NAME stellar7_moving_prefix_tests COMMAND stellar7_moving_prefix_tests)
target_compile_options(stellar7_moving_prefix_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
target_sources(stellar7_moving_prefix_tests PRIVATE tests/assertions_enabled.cpp)

add_executable(stellar7_moving_ground_tests tests/moving_ground_test.cpp)
target_link_libraries(stellar7_moving_ground_tests PRIVATE stellar7_core)
add_test(NAME stellar7_moving_ground_tests COMMAND stellar7_moving_ground_tests)
target_compile_options(stellar7_moving_ground_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>
)
target_sources(stellar7_moving_ground_tests PRIVATE tests/assertions_enabled.cpp)

if(WIN32)
    add_executable(stellar7_win32_audio_tests tests/win32_audio_test.cpp)
    target_link_libraries(stellar7_win32_audio_tests PRIVATE stellar7_core user32 gdi32 winmm)
    target_compile_options(stellar7_win32_audio_tests PRIVATE /UNDEBUG)
    target_sources(stellar7_win32_audio_tests PRIVATE tests/assertions_enabled.cpp)
    add_test(NAME stellar7_win32_audio_tests COMMAND stellar7_win32_audio_tests)
endif()

add_executable(stellar7_native_startup_tests tests/native_startup_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_native_startup_tests PRIVATE stellar7_core)
target_compile_options(stellar7_native_startup_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
target_link_options(stellar7_native_startup_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:4194304>)
add_test(NAME stellar7_native_startup_tests COMMAND stellar7_native_startup_tests)

add_executable(stellar7_ui_text_work_tests tests/ui_text_work_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_ui_text_work_tests PRIVATE stellar7_core)
target_compile_options(stellar7_ui_text_work_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_ui_text_work_tests COMMAND stellar7_ui_text_work_tests)

foreach(work_test menu_drawing_work ui_pointer_work menu_widget_work audio_program_fade audio_ended_once_direct level1_horizon_work level1_world_work level1_prefix_work level1_tail_work level1_frame level1_motion_render_work level1_banked_render_work gameplay_direct_event level1_primary_fire_work weapon_lamp_work level1_projectile_prefix_work level1_projectile_frame level1_primary_fire_tail level1_projectile_radar_work level1_primary_timer_tail level1_projectile_multi_prefix_work level1_projectile_multi_frame level1_enemy_prefix_work gameplay_enemy_event level1_enemy_frame level1_enemy_tail level1_player_hit_prefix_work gameplay_player_hit_event level1_zero_radius_render level1_projectile_reuse_work level1_actor_contact_work recovered_menu_input recovered_frontend gameplay_player_combat_event player_combat_lifecycle_work player_combat_lifecycle_frame level1_combat_armor_work failure_view_work failure_heap_work failure_effect_work failure_control_work failure_model_work failure_gameplay_work level2_failure_gameplay_work)
    add_executable(stellar7_${work_test}_tests tests/${work_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${work_test}_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_${work_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${work_test}_tests COMMAND stellar7_${work_test}_tests)
endforeach()

foreach(work_test gameplay_guardian_defeat_event guardian_effect_projection level1_success_tail_work level1_transition_work level2_music_work reward_radar_work reward_render_work)
    add_executable(stellar7_${work_test}_tests tests/${work_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${work_test}_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_${work_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${work_test}_tests COMMAND stellar7_${work_test}_tests)
endforeach()

foreach(work_test normal_work_packets level2_periodic_score_caller level2_periodic_lift_render level2_compact_bridge level2_periodic_bridge_model level2_periodic_target_contact level2_periodic_audio level2_periodic_render level2_periodic_model level2_periodic_spawn_work level2_lifecycle_work level2_type19_prefix_work level2_target_hit_work level2_score_work level2_projectile_prefix_work level2_projectile_model level2_combat_render_work level2_combat_tail_work level2_combat_tail_model shared_model_sphere_work level1_owned_view_work level1_multiple_lift_frame level1_retained_ordinary_frame level1_solid_contact_work gameplay_solid_contact_event level1_type3_work level1_type3_render_work)
    add_executable(stellar7_${work_test}_tests tests/${work_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${work_test}_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_${work_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${work_test}_tests COMMAND stellar7_${work_test}_tests)
endforeach()

# Explicit opt-in: supplied original resources are not part of the default tests.
set(STELLAR7_TEST_RESOURCE_DIR "" CACHE PATH "Supplied Stellar7 resources for controlled session tests")
if(STELLAR7_TEST_RESOURCE_DIR)
    foreach(resource COCKPIT.RES STELLAR.RES STELART.RES LEVEL1.RES LEVEL2.RES)
        if(NOT EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/${resource}")
            message(FATAL_ERROR "Missing supplied test resource: ${resource}. Legacy tests require these filenames in uppercase.")
        endif()
    endforeach()
    add_executable(stellar7_native_level2_retry_resource_tests
        tests/native_level2_retry_session_test.cpp
        tests/controlled_level2_retry_preloop_factory.cpp
        tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_native_level2_retry_resource_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_native_level2_retry_resource_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_native_level2_retry_resource_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    add_test(NAME stellar7_native_level2_retry_resource_tests
        COMMAND stellar7_native_level2_retry_resource_tests "${STELLAR7_TEST_RESOURCE_DIR}")
    set_tests_properties(stellar7_native_level2_retry_resource_tests PROPERTIES
        LABELS "resource-backed;controlled-session" TIMEOUT 1800)
    add_executable(stellar7_native_level2_combat_resource_tests
        tests/native_level2_combat_session_test.cpp
        tests/controlled_level2_preloop_factory_v1.cpp
        tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_native_level2_combat_resource_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_native_level2_combat_resource_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_native_level2_combat_resource_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    add_test(NAME stellar7_native_level2_combat_resource_tests
        COMMAND stellar7_native_level2_combat_resource_tests "${STELLAR7_TEST_RESOURCE_DIR}")
    set_tests_properties(stellar7_native_level2_combat_resource_tests PROPERTIES
        LABELS "resource-backed;controlled-session" TIMEOUT 1800)
    if(EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/LEVEL3.RES"
        AND EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/LEVEL4.RES"
        AND EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/LEVEL5.RES"
        AND EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/LEVEL6.RES"
        AND EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/LEVEL7.RES")
        add_executable(stellar7_playable_campaign_resource_tests
            tests/playable_campaign_session_test.cpp
            tests/controlled_level2_preloop_factory_v1.cpp tests/assertions_enabled.cpp)
        target_link_libraries(stellar7_playable_campaign_resource_tests PRIVATE stellar7_core)
        target_compile_options(stellar7_playable_campaign_resource_tests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
        target_link_options(stellar7_playable_campaign_resource_tests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
        add_test(NAME stellar7_playable_campaign_resource_tests
            COMMAND stellar7_playable_campaign_resource_tests "${STELLAR7_TEST_RESOURCE_DIR}")
        set_tests_properties(stellar7_playable_campaign_resource_tests PROPERTIES
            LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
        add_executable(stellar7_playable_campaign_continuity_tests
            tests/playable_campaign_continuity_test.cpp
            tests/controlled_level2_preloop_factory_v1.cpp tests/assertions_enabled.cpp)
        target_link_libraries(stellar7_playable_campaign_continuity_tests PRIVATE stellar7_core)
        target_compile_options(stellar7_playable_campaign_continuity_tests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
        target_link_options(stellar7_playable_campaign_continuity_tests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
        add_test(NAME stellar7_playable_campaign_continuity_tests
            COMMAND stellar7_playable_campaign_continuity_tests "${STELLAR7_TEST_RESOURCE_DIR}")
        set_tests_properties(stellar7_playable_campaign_continuity_tests PROPERTIES
            LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
        add_test(NAME stellar7_playable_level2_transition_audio_tests
            COMMAND stellar7_playable_campaign_continuity_tests "${STELLAR7_TEST_RESOURCE_DIR}" --audio-pass)
        set_tests_properties(stellar7_playable_level2_transition_audio_tests PROPERTIES
            LABELS "resource-backed;controlled-session;transition-audio" TIMEOUT 1800)
        add_executable(stellar7_playable_campaign_lifecycle_tests
            tests/playable_campaign_lifecycle_test.cpp
            tests/controlled_level2_preloop_factory_v1.cpp tests/assertions_enabled.cpp)
        target_link_libraries(stellar7_playable_campaign_lifecycle_tests PRIVATE stellar7_core)
        target_compile_options(stellar7_playable_campaign_lifecycle_tests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
        target_link_options(stellar7_playable_campaign_lifecycle_tests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
        add_test(NAME stellar7_playable_campaign_lifecycle_tests
            COMMAND stellar7_playable_campaign_lifecycle_tests "${STELLAR7_TEST_RESOURCE_DIR}")
        set_tests_properties(stellar7_playable_campaign_lifecycle_tests PROPERTIES
            LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
        if(EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/SCENED.RES" AND EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/VOICE.RES")
            add_executable(stellar7_playable_campaign_ending_tests
                tests/playable_campaign_ending_test.cpp
                tests/controlled_level2_preloop_factory_v1.cpp tests/assertions_enabled.cpp)
            target_link_libraries(stellar7_playable_campaign_ending_tests PRIVATE stellar7_core)
            target_compile_options(stellar7_playable_campaign_ending_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
                $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
            target_link_options(stellar7_playable_campaign_ending_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
            add_test(NAME stellar7_playable_campaign_ending_tests
                COMMAND stellar7_playable_campaign_ending_tests "${STELLAR7_TEST_RESOURCE_DIR}")
            set_tests_properties(stellar7_playable_campaign_ending_tests PROPERTIES
                LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
            add_executable(stellar7_playable_campaign_title_highscore_tests
                tests/playable_campaign_title_highscore_test.cpp
                tests/controlled_level2_preloop_factory_v1.cpp tests/assertions_enabled.cpp)
            target_link_libraries(stellar7_playable_campaign_title_highscore_tests PRIVATE stellar7_core)
            target_compile_options(stellar7_playable_campaign_title_highscore_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
                $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
            target_link_options(stellar7_playable_campaign_title_highscore_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
            add_test(NAME stellar7_playable_campaign_title_highscore_tests
                COMMAND stellar7_playable_campaign_title_highscore_tests "${STELLAR7_TEST_RESOURCE_DIR}")
            set_tests_properties(stellar7_playable_campaign_title_highscore_tests PROPERTIES
                LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
        endif()
        set(campaign_boundary_tests playable_campaign_blue_warp playable_campaign_admission playable_system_admission playable_type24_radar playable_fuel_bay playable_native_bonus playable_pointer_lifecycle playable_pointer_title playable_retired_guardian_emission playable_guardian_physical_old playable_warp_input)
        if(EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/SCENED.RES" AND EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/VOICE.RES")
            list(APPEND campaign_boundary_tests playable_campaign_native_guardians)
        endif()
        foreach(boundary_test IN LISTS campaign_boundary_tests)
            add_executable(stellar7_${boundary_test}_tests
                tests/${boundary_test}_test.cpp
                tests/controlled_level2_preloop_factory_v1.cpp tests/assertions_enabled.cpp)
            target_link_libraries(stellar7_${boundary_test}_tests PRIVATE stellar7_core)
            target_compile_options(stellar7_${boundary_test}_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
                $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
            target_link_options(stellar7_${boundary_test}_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
            add_test(NAME stellar7_${boundary_test}_tests
                COMMAND stellar7_${boundary_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}")
            set_tests_properties(stellar7_${boundary_test}_tests PROPERTIES
                LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
        endforeach()
        foreach(campaign_test playable_campaign_levels playable_level1_adoption playable_level3_progression
            playable_campaign_retry playable_campaign_guardians playable_type6_visibility playable_campaign_pilot
            playable_failure_bitmap playable_local_bank playable_public_startup playable_public_blue_route playable_system_pages playable_native_jump playable_jump_model level5_jump_bonus
            playable_failure_bank playable_failure_callback playable_timed_systems_model playable_hostile_damage playable_native_timed_systems playable_native_decoy
            playable_type4_shield_contact playable_type13_player_contact playable_level3_guardian_animation playable_object_status playable_bomb_model playable_native_bomb playable_bomb_adcc playable_menu playable_sounds briefing_viewer playable_menu_dwell playable_music_preferences music_publication playable_difficulty_preferences playable_mouse_preferences playable_detail_preferences playable_detail_ui)
            add_executable(stellar7_${campaign_test}_tests
                tests/${campaign_test}_test.cpp tests/assertions_enabled.cpp)
            target_link_libraries(stellar7_${campaign_test}_tests PRIVATE stellar7_core)
            if(STELLAR7_ENHANCED AND (campaign_test STREQUAL "playable_level3_guardian_animation"
                OR campaign_test STREQUAL "playable_object_status"))
                target_compile_definitions(stellar7_${campaign_test}_tests PRIVATE STELLAR7_ENHANCED=1)
            endif()
            if(campaign_test STREQUAL "playable_type6_visibility" OR campaign_test STREQUAL "playable_failure_bitmap"
                OR campaign_test STREQUAL "playable_system_pages" OR campaign_test STREQUAL "playable_jump_model" OR campaign_test STREQUAL "level5_jump_bonus"
                OR campaign_test STREQUAL "playable_failure_bank" OR campaign_test STREQUAL "playable_failure_callback"
                OR campaign_test STREQUAL "playable_timed_systems_model" OR campaign_test STREQUAL "playable_hostile_damage" OR campaign_test STREQUAL "playable_native_decoy" OR campaign_test STREQUAL "playable_bomb_model"
                OR campaign_test STREQUAL "playable_native_bomb" OR campaign_test STREQUAL "playable_bomb_adcc" OR campaign_test STREQUAL "playable_object_status" OR campaign_test STREQUAL "playable_level3_guardian_animation" OR campaign_test STREQUAL "briefing_viewer" OR campaign_test STREQUAL "playable_detail_ui")
                target_include_directories(stellar7_${campaign_test}_tests PRIVATE src)
            endif()
            target_compile_options(stellar7_${campaign_test}_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
                $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
            target_link_options(stellar7_${campaign_test}_tests PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
            add_test(NAME stellar7_${campaign_test}_tests
                COMMAND stellar7_${campaign_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}")
            set_tests_properties(stellar7_${campaign_test}_tests PROPERTIES
                LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
            if(campaign_test STREQUAL "playable_level3_progression")
                add_test(NAME stellar7_playable_level3_combat_retry_tests
                    COMMAND stellar7_${campaign_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}" --combat-retry)
                set_tests_properties(stellar7_playable_level3_combat_retry_tests PROPERTIES
                    LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
            endif()
        endforeach()
        set_tests_properties(stellar7_playable_public_blue_route_tests PROPERTIES
            LABELS "resource-backed;actual-startup;playable-campaign;natural-blue-route")
    endif()
endif()

set(STELLAR7_TEST_CAMPAIGN_REPLAY_DIR "" CACHE PATH "Qualified generated natural campaign replay evidence folder")
if(STELLAR7_TEST_RESOURCE_DIR AND STELLAR7_TEST_CAMPAIGN_REPLAY_DIR)
    if(NOT EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/SCENED.RES" OR NOT EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/VOICE.RES")
        message(FATAL_ERROR "Natural fullroute replay requires actual ending resources")
    endif()
    add_executable(stellar7_playable_public_campaign_replay_tests
        tests/playable_public_campaign_replay_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_playable_public_campaign_replay_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_playable_public_campaign_replay_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_playable_public_campaign_replay_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    add_test(NAME stellar7_playable_public_campaign_replay_tests
        COMMAND "${CMAKE_COMMAND}" "-DREPLAY_EXE=$<TARGET_FILE:stellar7_playable_public_campaign_replay_tests>"
            "-DRESOURCE_DIR=${STELLAR7_TEST_RESOURCE_DIR}" "-DREPLAY_DIR=${STELLAR7_TEST_CAMPAIGN_REPLAY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/playable_route_binding.cmake")
    set_tests_properties(stellar7_playable_public_campaign_replay_tests PROPERTIES
        LABELS "resource-backed;actual-startup;playable-campaign;natural-blue-ending;qualified-input-movie" TIMEOUT 1800)
endif()

foreach(work_test audio_level2_repeat_work audio_level2_repeat_clock menu_controller_repeat)
    add_executable(stellar7_${work_test}_tests tests/${work_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${work_test}_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_${work_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${work_test}_tests COMMAND stellar7_${work_test}_tests)
endforeach()
target_include_directories(stellar7_menu_controller_repeat_tests PRIVATE src)
target_link_options(stellar7_audio_level2_repeat_clock_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)

# Root composes in CMakeLists.txt; existing core source membership is unchanged.
add_executable(stellar7_level2_retry_music_work_tests
    tests/level2_retry_music_work_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_level2_retry_music_work_tests PRIVATE stellar7_core)
target_compile_options(stellar7_level2_retry_music_work_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
target_link_options(stellar7_level2_retry_music_work_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
add_test(NAME stellar7_level2_retry_music_work_tests COMMAND stellar7_level2_retry_music_work_tests)
#16MiB is the existing synthetic startup fixture test policy; product stack stays unchanged.


add_executable(stellar7_level2_retry_work_tests tests/level2_retry_work_test.cpp tests/assertions_enabled.cpp)
target_link_libraries(stellar7_level2_retry_work_tests PRIVATE stellar7_core)
target_compile_options(stellar7_level2_retry_work_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_level2_retry_work_tests COMMAND stellar7_level2_retry_work_tests)

add_executable(stellar7_playable_jump_tests tests/playable_jump_test.cpp tests/assertions_enabled.cpp)
target_include_directories(stellar7_playable_jump_tests PRIVATE src)
target_link_libraries(stellar7_playable_jump_tests PRIVATE stellar7_core)
target_compile_options(stellar7_playable_jump_tests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
add_test(NAME stellar7_playable_jump_tests COMMAND stellar7_playable_jump_tests)

foreach(ability_test playable_ability_audio playable_timed_systems playable_decoy playable_decoy_palette_clock playable_bomb)
    add_executable(stellar7_${ability_test}_tests tests/${ability_test}_test.cpp tests/assertions_enabled.cpp)
    target_include_directories(stellar7_${ability_test}_tests PRIVATE src)
    target_link_libraries(stellar7_${ability_test}_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_${ability_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_${ability_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    add_test(NAME stellar7_${ability_test}_tests COMMAND stellar7_${ability_test}_tests)
endforeach()

# A complete retained route needs only supplied resources and repository-owned inputs.
if(STELLAR7_TEST_RESOURCE_DIR)
    file(GLOB supplied_campaign_files RELATIVE "${STELLAR7_TEST_RESOURCE_DIR}"
        LIST_DIRECTORIES FALSE CONFIGURE_DEPENDS "${STELLAR7_TEST_RESOURCE_DIR}/*")
    string(TOUPPER "${supplied_campaign_files}" supplied_campaign_files)
    set(missing_campaign_resources "")
    foreach(resource LEVEL3.RES LEVEL4.RES LEVEL5.RES LEVEL6.RES LEVEL7.RES
        SCENEA.RES SCENEB1.RES SCENEB2.RES SCENEB3.RES SCENEB5.RES SCENEC.RES
        SCENED.RES SCENEE.RES VOICE.RES DRAXON.RES)
        if(NOT resource IN_LIST supplied_campaign_files)
            list(APPEND missing_campaign_resources "${resource}")
        endif()
    endforeach()
    if(missing_campaign_resources)
        list(JOIN missing_campaign_resources ", " missing_campaign_resources)
        message(STATUS "Full campaign test unavailable. Missing resources: ${missing_campaign_resources}")
    else()
        add_executable(stellar7_playable_full_campaign_tests
            tests/playable_full_campaign_test.cpp tests/assertions_enabled.cpp)
        target_include_directories(stellar7_playable_full_campaign_tests PRIVATE src)
        target_link_libraries(stellar7_playable_full_campaign_tests PRIVATE stellar7_assets stellar7_player)
        target_compile_options(stellar7_playable_full_campaign_tests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
        target_link_options(stellar7_playable_full_campaign_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
        foreach(difficulty easy standard hard)
            if(difficulty STREQUAL "easy")
                set(campaign_test_name stellar7_playable_full_campaign_tests)
            else()
                set(campaign_test_name stellar7_playable_${difficulty}_campaign_tests)
            endif()
            set(campaign_fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/${difficulty}_campaign")
            add_test(NAME "${campaign_test_name}" COMMAND stellar7_playable_full_campaign_tests
                "${STELLAR7_TEST_RESOURCE_DIR}" "${campaign_fixture}.inputs.txt"
                "${campaign_fixture}.expected.txt" "${difficulty}")
            set_tests_properties("${campaign_test_name}" PROPERTIES
                LABELS "resource-backed;actual-startup;playable-campaign;full-campaign" TIMEOUT 1800)
        endforeach()
    endif()
endif()

# New opt-in target only; old delivered3904-route target/fixture stay unchanged.
set(STELLAR7_TEST_SEQUENTIAL_REPLAY_DIR "" CACHE PATH "Qualified actual-startup sequential4/5/6/7 input and semantic evidence")
if(STELLAR7_TEST_RESOURCE_DIR AND STELLAR7_TEST_SEQUENTIAL_REPLAY_DIR)
    if(NOT EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/SCENED.RES" OR NOT EXISTS "${STELLAR7_TEST_RESOURCE_DIR}/VOICE.RES")
        message(FATAL_ERROR "Sequential campaign replay requires actual ending resources")
    endif()
    add_executable(stellar7_playable_sequential_campaign_replay_tests
        tests/playable_sequential_campaign_replay_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_playable_sequential_campaign_replay_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_playable_sequential_campaign_replay_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_playable_sequential_campaign_replay_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    add_test(NAME stellar7_playable_sequential_campaign_replay_tests
        COMMAND "${CMAKE_COMMAND}" "-DREPLAY_EXE=$<TARGET_FILE:stellar7_playable_sequential_campaign_replay_tests>"
            "-DRESOURCE_DIR=${STELLAR7_TEST_RESOURCE_DIR}" "-DREPLAY_DIR=${STELLAR7_TEST_SEQUENTIAL_REPLAY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/playable_route_binding.cmake")
    set_tests_properties(stellar7_playable_sequential_campaign_replay_tests PROPERTIES
        LABELS "resource-backed;actual-startup;playable-campaign;natural-sequential-victories;qualified-input-movie" TIMEOUT 1800)
endif()

if(STELLAR7_TEST_RESOURCE_DIR)
    add_executable(stellar7_playable_bomb_audio_tests tests/playable_bomb_audio_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_playable_bomb_audio_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_playable_bomb_audio_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_playable_bomb_audio_tests COMMAND stellar7_playable_bomb_audio_tests "${STELLAR7_TEST_RESOURCE_DIR}")
endif()

foreach(preference_test campaign_difficulty playable_pointer playable_detail_render)
    add_executable(stellar7_${preference_test}_tests tests/${preference_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${preference_test}_tests PRIVATE stellar7_core)
    target_include_directories(stellar7_${preference_test}_tests PRIVATE src)
    target_compile_options(stellar7_${preference_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_${preference_test}_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    add_test(NAME stellar7_${preference_test}_tests COMMAND stellar7_${preference_test}_tests)
endforeach()

set(native_release_tests highscore_staged)
if(STELLAR7_TEST_RESOURCE_DIR)
    list(APPEND native_release_tests playable_menu_idle_clock playable_menu_idle
        playable_save_failure playable_rank_save_failure)
endif()
foreach(release_test IN LISTS native_release_tests)
    add_executable(stellar7_${release_test}_tests tests/${release_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${release_test}_tests PRIVATE stellar7_core)
    target_include_directories(stellar7_${release_test}_tests PRIVATE src)
    target_compile_options(stellar7_${release_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_${release_test}_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    if(release_test STREQUAL "playable_rank_save_failure")
        target_sources(stellar7_${release_test}_tests PRIVATE tests/controlled_level2_preloop_factory_v1.cpp)
    endif()
    if(release_test STREQUAL "highscore_staged")
        add_test(NAME stellar7_${release_test}_tests
            COMMAND stellar7_${release_test}_tests "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(stellar7_${release_test}_tests PROPERTIES LABELS "native-store" TIMEOUT 1800)
    elseif(release_test STREQUAL "playable_save_failure")
        add_test(NAME stellar7_${release_test}_tests
            COMMAND stellar7_${release_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")
        set_tests_properties(stellar7_${release_test}_tests PROPERTIES
            LABELS "resource-backed;actual-startup;playable-campaign" TIMEOUT 1800)
    else()
        add_test(NAME stellar7_${release_test}_tests
            COMMAND stellar7_${release_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}")
        set_tests_properties(stellar7_${release_test}_tests PROPERTIES
            LABELS "resource-backed;controlled-session;playable-campaign" TIMEOUT 1800)
    endif()
endforeach()

set(story_detail_tests playable_full_detail_render playable_full_detail_model playable_story_loader_alias)
if(STELLAR7_TEST_RESOURCE_DIR)
    list(APPEND story_detail_tests story_scene playable_story_preferences playable_story_background playable_game_over_scene playable_full_detail_preferences
        playable_live_preferences playable_live_preferences_ui playable_live_detail_range
        playable_detail_menu_display playable_detail_live_display playable_live_play_layout)
endif()
foreach(menu_test IN LISTS story_detail_tests)
    add_executable(stellar7_${menu_test}_tests tests/${menu_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${menu_test}_tests PRIVATE stellar7_core)
    target_include_directories(stellar7_${menu_test}_tests PRIVATE src)
    target_compile_options(stellar7_${menu_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_${menu_test}_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    add_test(NAME stellar7_${menu_test}_tests COMMAND stellar7_${menu_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}")
    set_tests_properties(stellar7_${menu_test}_tests PROPERTIES LABELS "menu-features" TIMEOUT 1800)
endforeach()

if(STELLAR7_TEST_RESOURCE_DIR)
    add_test(NAME stellar7_playable_story_audio_modes_tests
        COMMAND stellar7_playable_story_preferences_tests "${STELLAR7_TEST_RESOURCE_DIR}" audio-modes)
    set_tests_properties(stellar7_playable_story_audio_modes_tests PROPERTIES LABELS "menu-features" TIMEOUT 1800)
endif()

set(joystick_attract_tests native_input_qualification playable_joystick_cancel playable_joystick playable_attract_controller)
if(WIN32)
    list(APPEND joystick_attract_tests win32_joystick_host)
endif()
if(STELLAR7_TEST_RESOURCE_DIR)
    list(APPEND joystick_attract_tests playable_joystick_calibration menu_calibration_checkpoint playable_attract_borrow_guard playable_attract_clock playable_attract_route)
endif()
foreach(menu_test IN LISTS joystick_attract_tests)
    add_executable(stellar7_${menu_test}_tests tests/${menu_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${menu_test}_tests PRIVATE stellar7_core)
    target_include_directories(stellar7_${menu_test}_tests PRIVATE src)
    target_compile_options(stellar7_${menu_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_${menu_test}_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:16777216>)
    if(menu_test STREQUAL "playable_attract_route")
        add_test(NAME stellar7_${menu_test}_tests COMMAND stellar7_${menu_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}" 0)
        add_test(NAME stellar7_playable_attract_story_route_tests COMMAND stellar7_${menu_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}" 255)
        set_tests_properties(stellar7_playable_attract_story_route_tests PROPERTIES LABELS "native-menu-input" TIMEOUT 1800)
    else()
        add_test(NAME stellar7_${menu_test}_tests COMMAND stellar7_${menu_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}")
    endif()
    set_tests_properties(stellar7_${menu_test}_tests PROPERTIES LABELS "native-menu-input" TIMEOUT 1800)
endforeach()

foreach(host_contract audio input host audio_device joystick)
    set(host_test sdl2_${host_contract})
    if(host_contract STREQUAL "audio")
        set(host_test_source tests/${host_test}_test.cpp)
    else()
        set(host_test_source tests/sdl_${host_contract}_test.cpp)
    endif()
    add_executable(stellar7_${host_test}_tests ${host_test_source} tests/assertions_enabled.cpp)
    target_include_directories(stellar7_${host_test}_tests PRIVATE src)
    target_link_libraries(stellar7_${host_test}_tests PRIVATE stellar7_core stellar7_sdl)
    target_compile_definitions(stellar7_${host_test}_tests PRIVATE SDL_MAIN_HANDLED)
    target_compile_options(stellar7_${host_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    add_test(NAME stellar7_${host_test}_tests COMMAND stellar7_${host_test}_tests)
    set_tests_properties(stellar7_${host_test}_tests PROPERTIES TIMEOUT 60)
    if(host_contract STREQUAL "host" OR host_contract STREQUAL "audio_device")
        set_tests_properties(stellar7_${host_test}_tests PROPERTIES ENVIRONMENT
            "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
    endif()
endforeach()
foreach(resource_host_test story_streaming native_frame_pacing cold_presentation_pacing playable_cockpit_timing cold_title_sequence title_copy_clock native_title_pacing)
    add_executable(stellar7_${resource_host_test}_tests tests/${resource_host_test}_test.cpp tests/assertions_enabled.cpp)
    target_link_libraries(stellar7_${resource_host_test}_tests PRIVATE stellar7_core)
    if(resource_host_test STREQUAL "title_copy_clock")
        target_include_directories(stellar7_${resource_host_test}_tests PRIVATE src)
    endif()
    target_compile_options(stellar7_${resource_host_test}_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_${resource_host_test}_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:4194304>)
    if(STELLAR7_TEST_RESOURCE_DIR)
        add_test(NAME stellar7_${resource_host_test}_tests COMMAND stellar7_${resource_host_test}_tests "${STELLAR7_TEST_RESOURCE_DIR}")
    endif()
endforeach()

if(STELLAR7_TEST_RESOURCE_DIR)
    set(player_audio_targets)
    foreach(audio_target playable_campaign_continuity playable_campaign_ending story_scene
        playable_story_background playable_game_over_scene playable_story_preferences)
        if(TARGET stellar7_${audio_target}_tests)
            list(APPEND player_audio_targets stellar7_${audio_target}_tests)
        endif()
    endforeach()
    foreach(audio_test playable_level2_transition_audio playable_campaign_ending story_scene
        playable_story_background playable_game_over_scene playable_story_audio_modes)
        if(TEST stellar7_${audio_test}_tests)
            set_property(TEST stellar7_${audio_test}_tests APPEND PROPERTY LABELS player-journey-audio)
        endif()
    endforeach()
    add_custom_target(stellar7_check_audio_routes
        COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> -L player-journey-audio --output-on-failure
        DEPENDS ${player_audio_targets}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        USES_TERMINAL)
endif()

set(STELLAR7_TEST_LEVEL1_SPAWN_ORACLE "" CACHE FILEPATH "Qualified private original Level1 spawn oracle header")
if(STELLAR7_TEST_RESOURCE_DIR AND STELLAR7_TEST_LEVEL1_SPAWN_ORACLE)
    if(NOT EXISTS "${STELLAR7_TEST_LEVEL1_SPAWN_ORACLE}")
        message(FATAL_ERROR "Level1 spawn regression requires its qualified original oracle")
    endif()
    add_executable(stellar7_playable_level1_spawn_regression_tests
        tests/playable_level1_spawn_regression_test.cpp tests/assertions_enabled.cpp)
    target_compile_definitions(stellar7_playable_level1_spawn_regression_tests PRIVATE
        "STELLAR7_LEVEL1_SPAWN_ORACLE=\"${STELLAR7_TEST_LEVEL1_SPAWN_ORACLE}\"")
    target_link_libraries(stellar7_playable_level1_spawn_regression_tests PRIVATE stellar7_core)
    target_compile_options(stellar7_playable_level1_spawn_regression_tests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)
    target_link_options(stellar7_playable_level1_spawn_regression_tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/STACK:4194304>)
    add_test(NAME stellar7_playable_level1_spawn_regression_tests
        COMMAND stellar7_playable_level1_spawn_regression_tests "${STELLAR7_TEST_RESOURCE_DIR}")
    set_tests_properties(stellar7_playable_level1_spawn_regression_tests PROPERTIES
        LABELS "resource-backed;actual-startup;qualified-state" TIMEOUT 180)
endif()
