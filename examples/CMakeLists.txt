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
)

# 批量创建示例程序
foreach(example ${EXAMPLES})
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} RenderEngine)
    
    # 为所有示例程序设置 UTF-8 编码（支持中文注释和字符串）
    if(MSVC)
        target_compile_options(${example} PRIVATE /utf-8)
    endif()
endforeach()

# 创建一个自定义目标来管理着色器文件的复制
# 这样只有当着色器文件实际变化时才会触发复制
file(GLOB_RECURSE SHADER_FILES "${CMAKE_SOURCE_DIR}/shaders/*")

add_custom_target(copy_shaders
    COMMENT "Checking shader files..."
)

foreach(shader_file ${SHADER_FILES})
    file(RELATIVE_PATH rel_path ${CMAKE_SOURCE_DIR}/shaders ${shader_file})
    add_custom_command(TARGET copy_shaders
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${shader_file}
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${rel_path}
        DEPENDS ${shader_file}
        COMMENT "Copying ${rel_path}"
    )
endforeach()

# 复制 SDL3 DLL 到输出目录（Windows）
if(WIN32 AND SDL3_LIBRARY)
    get_filename_component(SDL3_DLL_DIR ${SDL3_LIBRARY} DIRECTORY)
    file(GLOB SDL3_DLLS "${SDL3_DLL_DIR}/*.dll")
    
    foreach(dll ${SDL3_DLLS})
        get_filename_component(dll_name ${dll} NAME)
        add_custom_command(TARGET copy_shaders
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${dll}
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${dll_name}
            DEPENDS ${dll}
            COMMENT "Copying ${dll_name}"
        )
    endforeach()
endif()

# 让所有示例程序依赖于 copy_shaders 目标
foreach(example ${EXAMPLES})
    add_dependencies(${example} copy_shaders)
endforeach()

