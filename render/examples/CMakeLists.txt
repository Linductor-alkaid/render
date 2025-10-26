# 示例程序

# 基础窗口测试
add_executable(01_basic_window 01_basic_window.cpp)
target_link_libraries(01_basic_window RenderEngine)

# 复制 SDL3 DLL 到输出目录（Windows）
if(WIN32 AND SDL3_LIBRARY)
    get_filename_component(SDL3_DLL_DIR ${SDL3_LIBRARY} DIRECTORY)
    file(GLOB SDL3_DLLS "${SDL3_DLL_DIR}/*.dll")
    
    foreach(dll ${SDL3_DLLS})
        add_custom_command(TARGET 01_basic_window POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${dll}
            $<TARGET_FILE_DIR:01_basic_window>
        )
    endforeach()
endif()

