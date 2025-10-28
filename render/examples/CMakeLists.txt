# 示例程序

# 01 - 基础窗口测试
add_executable(01_basic_window 01_basic_window.cpp)
target_link_libraries(01_basic_window RenderEngine)

# 02 - 着色器系统测试
add_executable(02_shader_test 02_shader_test.cpp)
target_link_libraries(02_shader_test RenderEngine)

# 03 - 几何着色器和着色器缓存测试
add_executable(03_geometry_shader_test 03_geometry_shader_test.cpp)
target_link_libraries(03_geometry_shader_test RenderEngine)

# 04 - 状态管理测试
add_executable(04_state_management_test 04_state_management_test.cpp)
target_link_libraries(04_state_management_test RenderEngine)
# 设置 UTF-8 编码
if(MSVC)
    target_compile_options(04_state_management_test PRIVATE /utf-8)
endif()

# 05 - 纹理加载测试
add_executable(05_texture_test 05_texture_test.cpp)
target_link_libraries(05_texture_test RenderEngine)

# 06 - 网格系统测试
add_executable(06_mesh_test 06_mesh_test.cpp)
target_link_libraries(06_mesh_test RenderEngine)
# 设置 UTF-8 编码
if(MSVC)
    target_compile_options(06_mesh_test PRIVATE /utf-8)
endif()

# 07 - 着色器系统线程安全测试
add_executable(07_thread_safe_test 07_thread_safe_test.cpp)
target_link_libraries(07_thread_safe_test RenderEngine)
# 设置 UTF-8 编码
if(MSVC)
    target_compile_options(07_thread_safe_test PRIVATE /utf-8)
endif()

# 08 - Renderer 线程安全测试
add_executable(08_renderer_thread_safe_test 08_renderer_thread_safe_test.cpp)
target_link_libraries(08_renderer_thread_safe_test RenderEngine)
# 设置 UTF-8 编码
if(MSVC)
    target_compile_options(08_renderer_thread_safe_test PRIVATE /utf-8)
endif()

# 09 - 纹理系统线程安全测试
add_executable(09_texture_thread_safe_test 09_texture_thread_safe_test.cpp)
target_link_libraries(09_texture_thread_safe_test RenderEngine)
# 设置 UTF-8 编码
if(MSVC)
    target_compile_options(09_texture_thread_safe_test PRIVATE /utf-8)
endif()

# 10 - 网格系统线程安全测试
add_executable(10_mesh_thread_safe_test 10_mesh_thread_safe_test.cpp)
target_link_libraries(10_mesh_thread_safe_test RenderEngine)
# 设置 UTF-8 编码
if(MSVC)
    target_compile_options(10_mesh_thread_safe_test PRIVATE /utf-8)
endif()

# 复制 SDL3 DLL 到输出目录（Windows）
if(WIN32 AND SDL3_LIBRARY)
    get_filename_component(SDL3_DLL_DIR ${SDL3_LIBRARY} DIRECTORY)
    file(GLOB SDL3_DLLS "${SDL3_DLL_DIR}/*.dll")
    
    set(ALL_EXAMPLES 01_basic_window 02_shader_test 03_geometry_shader_test 04_state_management_test 05_texture_test 06_mesh_test 07_thread_safe_test 08_renderer_thread_safe_test 09_texture_thread_safe_test 10_mesh_thread_safe_test)
    
    foreach(example ${ALL_EXAMPLES})
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

