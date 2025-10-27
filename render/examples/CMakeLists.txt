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

# 复制 SDL3 DLL 到输出目录（Windows）
if(WIN32 AND SDL3_LIBRARY)
    get_filename_component(SDL3_DLL_DIR ${SDL3_LIBRARY} DIRECTORY)
    file(GLOB SDL3_DLLS "${SDL3_DLL_DIR}/*.dll")
    
    set(ALL_EXAMPLES 01_basic_window 02_shader_test 03_geometry_shader_test)
    
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

