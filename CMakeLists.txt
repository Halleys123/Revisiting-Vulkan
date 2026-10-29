cmake_minimum_required(VERSION 4.3)

project(Vulkan)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED COMPONENTS glslangValidator)

set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")

add_subdirectory(externals/GLFW)
add_subdirectory(externals/glm)

add_executable(main ./main.cpp ./src/VulkanRenderer.cpp)
target_include_directories(main PRIVATE ./include)

if(MSVC)
    target_link_options(main PRIVATE "/SUBSYSTEM:CONSOLE")
      target_compile_options(main PRIVATE /EHsc)
endif()

target_compile_definitions(main PRIVATE
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    VK_USE_PLATFORM_WIN32_KHR # Although it can be used, try not to use this
    $<$<CONFIG:Debug>:DEBUG_MODE>
    NOMINMAX
)

target_link_libraries(main PRIVATE glfw)
target_link_libraries(main PRIVATE glm::glm)
target_link_libraries(main PRIVATE Vulkan::Vulkan)

# CMake below this part is written by Claude and I don't understand what is happening here
# Research more about CMake after learning VUlkan
set(ASSET_SRC_DIR  ${CMAKE_SOURCE_DIR}/assets)
set(SHADER_SRC_DIR ${ASSET_SRC_DIR}/shaders)
set(SHADER_GEN_DIR ${CMAKE_BINARY_DIR}/generated/shaders)

# ---- 1. Compile shaders into build/generated/shaders ----
file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    ${SHADER_SRC_DIR}/*.vert
    ${SHADER_SRC_DIR}/*.frag
    ${SHADER_SRC_DIR}/*.comp
    ${SHADER_SRC_DIR}/*.geom
    ${SHADER_SRC_DIR}/*.tesc
    ${SHADER_SRC_DIR}/*.tese
)

set(SPV_FILES "")

foreach(SRC ${SHADER_SOURCES})
    file(RELATIVE_PATH REL ${SHADER_SRC_DIR} ${SRC})
    set(OUT ${SHADER_GEN_DIR}/${REL}.spv)
    get_filename_component(OUT_DIR ${OUT} DIRECTORY)

    add_custom_command(
        OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
        COMMAND Vulkan::glslangValidator -V ${SRC} -o ${OUT}
        DEPENDS ${SRC}
        COMMENT "Compiling shader ${REL}"
        VERBATIM
    )
    list(APPEND SPV_FILES ${OUT})
endforeach()

add_custom_target(shaders DEPENDS ${SPV_FILES})

# ---- 2. Copy non-shader asset folders next to the exe ----
file(GLOB ASSET_ENTRIES LIST_DIRECTORIES true CONFIGURE_DEPENDS ${ASSET_SRC_DIR}/*)

set(ASSET_COPY_COMMANDS "")
foreach(ENTRY ${ASSET_ENTRIES})
    if(IS_DIRECTORY ${ENTRY})
        get_filename_component(ENTRY_NAME ${ENTRY} NAME)
        if(NOT ENTRY_NAME STREQUAL "shaders")
            list(APPEND ASSET_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                        ${ENTRY} $<TARGET_FILE_DIR:main>/assets/${ENTRY_NAME}
            )
        endif()
    endif()
endforeach()

# ---- 3. Copy compiled .spv files into assets/shaders next to the exe ----
add_custom_target(copy_assets ALL
    ${ASSET_COPY_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_GEN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            ${SHADER_GEN_DIR} $<TARGET_FILE_DIR:main>/assets/shaders
)
add_dependencies(copy_assets shaders)
add_dependencies(main copy_assets)

target_compile_definitions(main PRIVATE ASSET_DIR="$<TARGET_FILE_DIR:main>")

# install(TARGETS main RUNTIME DESTINATION bin)
# set(CPACK_PACKAGE_NAME "VulkanApp")
# set(CPACK_PACKAGE_VERSION "1.0.0")
# set(CPACK_GENERATOR "ZIP") # Can be ZIP, NSIS (installer), TGZ, etc.
# include(CPack)
