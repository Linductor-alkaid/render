# 示例程序

# 定义所有示例程序
set(EXAMPLES
    01_basic_window
    02_shader_test
    03_geometry_shader_test
    04_state_management_test
    05_texture_test
    06_mesh_test
    07_thread_safe_test
    08_renderer_thread_safe_test
    09_texture_thread_safe_test
    10_mesh_thread_safe_test
    11_model_loader_test
    12_material_test
    13_material_thread_safe_test
    14_model_material_loader_test
    15_resource_manager_test
    16_resource_manager_thread_safe_test
    17_model_with_resource_manager_test
    18_math_test
    19_math_benchmark
    20_camera_test
    21_transform_thread_safe_test
    22_gl_thread_safety_test
    23_logger_test
    24_async_logger_test
    25_test_window_resize_callback
    26_test_eigen_alignment
    27_test_resource_handle
    28_circular_reference_detection_test
    29_async_loading_test
    30_framebuffer_test
    31_ecs_basic_test
    32_ecs_renderer_test
    33_ecs_async_test
    34_postprocess_test
    35_ecs_comprehensive_test
    36_ecs_transform_safety_test
    37_batching_benchmark
    38_sprite_render_test
    39_sprite_api_test
    40_sprite_animation_test
    41_sprite_batch_test
    42_sprite_state_machine_test
    43_sprite_batch_validation_test
    44_text_render_test
    45_lighting_test
    46_normal_map_test
    47_skeleton_palette_test
    48_model_render_test
    49_miku_model_test
    50_geometry_catalog_test
    51_layer_mask_demo
    52_application_boot_demo
    53_event_system_test
    54_module_hud_test
    55_scene_serialization_test
    56_scene_manager_stress_test
    57_toolchain_integration_test
    60_ui_framework_showcase
    61_ui_menu_example
)

# 批量创建示例程序
foreach(example ${EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} RenderEngine)
    
    # 为所有示例程序设置 UTF-8 编码（支持中文注释和字符串）
    if(MSVC)
        target_compile_options(${example} PRIVATE /utf-8)
    endif()
    
    # 为数学基准测试程序启用 AVX2 优化
    if(example STREQUAL "19_math_benchmark")
        if(MSVC)
            target_compile_options(${example} PRIVATE /arch:AVX2)
        else()
            target_compile_options(${example} PRIVATE -mavx2 -mfma)
        endif()
    endif()
endforeach()

# 复制 SDL3 DLL 和着色器到输出目录（Windows）
if(WIN32 AND SDL3_LIBRARY)
    get_filename_component(SDL3_DLL_DIR ${SDL3_LIBRARY} DIRECTORY)
    file(GLOB SDL3_DLLS "${SDL3_DLL_DIR}/*.dll")
    
    foreach(example ${EXAMPLES})
        foreach(dll ${SDL3_DLLS})
            add_custom_command(TARGET ${example} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${dll}
                $<TARGET_FILE_DIR:${example}>
            )
        endforeach()
        
        # 复制 shaders 目录到可执行文件目录
        add_custom_command(TARGET ${example} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:${example}>/shaders
        )
    endforeach()
endif()

