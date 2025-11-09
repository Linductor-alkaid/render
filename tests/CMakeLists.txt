# Phase 1 渲染相关测试

file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" PROJECT_SOURCE_DIR_POSIX)

add_executable(mesh_tangent_test mesh_tangent_test.cpp)
add_executable(model_loader_regression_test model_loader_regression_test.cpp)

target_link_libraries(mesh_tangent_test PRIVATE RenderEngine)
target_link_libraries(model_loader_regression_test PRIVATE RenderEngine)

target_compile_definitions(mesh_tangent_test PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR_POSIX}")
target_compile_definitions(model_loader_regression_test PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR_POSIX}")

if(MSVC)
    target_compile_options(mesh_tangent_test PRIVATE /utf-8)
    target_compile_options(model_loader_regression_test PRIVATE /utf-8)
endif()

add_test(NAME mesh_tangent_test COMMAND mesh_tangent_test)
add_test(NAME model_loader_regression_test COMMAND model_loader_regression_test)

