project(llreshade)

include(00-Common)

# ---- ReShade Source Files ----

# Effect Compiler
set(reshade_EFFECT_COMPILER
    effect_codegen_glsl.cpp
    effect_expression.cpp
    effect_lexer.cpp
    effect_parser_exp.cpp
    effect_parser_stmt.cpp
    effect_preprocessor.cpp
    effect_symbol_table.cpp
)

# Runtime Core
set(reshade_RUNTIME
    runtime.cpp
    runtime_api.cpp
    runtime_gui.cpp
    runtime_manager.cpp
)

# Infrastructure
set(reshade_INFRA
    ini_file.cpp
    dll_log.cpp
    dll_resources.cpp
    platform_utils_windows.cpp
)

# Addon System
set(reshade_ADDON
    addon.cpp
    addon_manager.cpp
)

# Input (MikoStorm simplified, no hook_manager dependency)
set(reshade_INPUT
    input.cpp
    input_windows_mikostorm.cpp
)

# MikoStorm Adapter
set(reshade_ADAPTER
    mikostorm_reshade_adapter.cpp
)

# State Block
set(reshade_STATE
    state_block.cpp
)

# ImGui Widgets / Code Editor
set(reshade_IMGUI_SRC
    imgui_code_editor.cpp
    imgui_function_table.cpp
    imgui_function_table_18600.cpp
    imgui_function_table_18971.cpp
    imgui_function_table_19000.cpp
    imgui_function_table_19040.cpp
    imgui_function_table_19180.cpp
    imgui_function_table_19191.cpp
    imgui_function_table_19222.cpp
    imgui_function_table_19250.cpp
    imgui_widgets.cpp
)

# ---- OpenGL Backend Implementation ----
set(reshade_OPENGL
    opengl/opengl_impl_device.cpp
    opengl/opengl_impl_command_list.cpp
    opengl/opengl_impl_command_queue.cpp
    opengl/opengl_impl_swapchain.cpp
    opengl/opengl_impl_state_block.cpp
    opengl/opengl_impl_type_convert.cpp
)

# ---- NOT included (hook layer, replaced by native integration): ----
# opengl/opengl_hooks_wgl.cpp   - WGL hooks (replaced by direct calls)
# opengl/opengl_hooks_ffp.cpp   - FFP hooks (not needed)
# opengl/opengl.cpp             - GL resource tracking hooks (not needed)
# opengl/opengl_hooks.hpp       - Hook declarations (not needed)
# input_windows.cpp             - Windows input hooks (replaced by viewer input)
# dll_main.cpp                  - DLL entry point (replaced by native init)

# ---- Headers (for IDE integration) ----
set(reshade_HEADERS
    version.h
    mikostorm_reshade_adapter.hpp
    runtime.hpp
    runtime_internal.hpp
    runtime_manager.hpp
    effect_codegen.hpp
    effect_expression.hpp
    effect_lexer.hpp
    effect_module.hpp
    effect_parser.hpp
    effect_preprocessor.hpp
    effect_symbol_table.hpp
    effect_token.hpp
    ini_file.hpp
    dll_log.hpp
    dll_resources.hpp
    platform_utils.hpp
    reshade_api_object_impl.hpp
    addon.hpp
    addon_manager.hpp
    input.hpp
    state_block.hpp
    localization.hpp
    imgui_code_editor.hpp
    imgui_function_table_19191.hpp
    imgui_widgets.hpp
    opengl/opengl_impl_device.hpp
    opengl/opengl_impl_state_block.hpp
    opengl/opengl_impl_swapchain.hpp
    opengl/opengl_impl_type_convert.hpp
    opengl/opengl_hooks.hpp
)

# ---- GLAD (OpenGL Function Loader) ----
set(reshade_GLAD_SRC
    deps/glad/src/gl.c
    deps/glad/src/wgl.c
)

# ---- ImGui Core ----
set(reshade_IMGUI_CORE
    deps/imgui/imgui.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_impl_opengl3.cpp
    deps/imgui/imgui_impl_win32.cpp
    deps/imgui_config.cpp
)

# ---- STB (Image Loading/Writing/Resizing) ----
set(reshade_STB_SRC
    deps/stb/stb_impl.cpp
)

# ---- FPNG (Fast PNG) ----
set(reshade_FPNG_SRC
    deps/fpng/fpng.cpp
)

# ---- JXL Simple Lossless ----
set(reshade_JXL_SRC
    deps/jxl_simple_lossless/simple_lossless.cpp
)

# ---- Collect all sources ----
list(APPEND reshade_ALL_SOURCES
    ${reshade_EFFECT_COMPILER}
    ${reshade_RUNTIME}
    ${reshade_INFRA}
    ${reshade_ADDON}
    ${reshade_INPUT}
    ${reshade_STATE}
    ${reshade_IMGUI_SRC}
    ${reshade_OPENGL}
    ${reshade_GLAD_SRC}
    ${reshade_IMGUI_CORE}
    ${reshade_STB_SRC}
    ${reshade_FPNG_SRC}
    ${reshade_JXL_SRC}
    ${reshade_ADAPTER}
    ${reshade_HEADERS}
)

# ---- Build as static library ----
add_library(llreshade STATIC ${reshade_ALL_SOURCES})

# ---- Include Directories ----
target_include_directories(llreshade
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/fpng
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/utfcpp
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/jxl_simple_lossless
)

# ---- Compile Definitions ----
target_compile_definitions(llreshade PRIVATE
    RESHADE_GUI
    RESHADE_GL
    RESHADE_OPENGL_IMPORT_WITH_VULKAN=0
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

    # MSVC-specific: suppress warnings from third-party code
if(MSVC)
    target_compile_options(llreshade PRIVATE
        /W0
        /WX-
        /Zc:char8_t-
        /wd4100   # unreferenced formal parameter
        /wd4127   # conditional expression is constant
        /wd4189   # local variable is initialized but not referenced
        /wd4244   # conversion from 'type1' to 'type2', possible loss of data
        /wd4267   # conversion from 'size_t' to 'type', possible loss of data
        /wd4389   # signed/unsigned mismatch
        /wd4456   # declaration hides previous local declaration
        /wd4457   # declaration hides function parameter
        /wd4458   # declaration hides class member
        /wd4459   # declaration hides global declaration
        /wd4505   # unreferenced local function has been removed
        /wd4702   # unreachable code
        /wd4706   # assignment within conditional expression
        /wd4838   # conversion from 'type' to 'type', requires a narrowing conversion
    )
    # Disable RTTI and exceptions (matching ReShade's build)
    target_compile_options(llreshade PRIVATE /GR- /EHs-c-)
endif()

# ---- Link Dependencies ----
target_link_libraries(llreshade
    PRIVATE
        opengl32
        user32
        dwmapi
        shlwapi
        shell32
)

# ---- Build GLAD as part of llreshade ----
# GLAD source files are already in the source list above

set_target_properties(llreshade PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
