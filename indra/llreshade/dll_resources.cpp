/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 *
 * MikoStorm: Replaced Win32 resource loading with embedded GLSL shaders
 * for native static library integration.
 */

#include "dll_resources.hpp"
#include "resource.h"
#include <cstring>
#include <Windows.h>

extern HMODULE g_module_handle;

static const char s_imgui_vs_glsl[] =
    "#version 430\n"
    "\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec2 tex;\n"
    "layout(location = 2) in vec4 col;\n"
    "\n"
    "out vec4 frag_col;\n"
    "out vec2 frag_tex;\n"
    "\n"
    "layout(binding = 0) uniform PushConstants\n"
    "{\n"
    "\tmat4 ortho_projection;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "\tfrag_col = col;\n"
    "\tfrag_tex = tex;\n"
    "\tgl_Position = ortho_projection * vec4(pos.xy, 0, 1);\n"
    "}\n";

static const char s_imgui_ps_glsl[] =
    "#version 430\n"
    "\n"
    "layout(binding = 0) uniform sampler2D s0;\n"
    "\n"
    "in vec4 frag_col;\n"
    "in vec2 frag_tex;\n"
    "\n"
    "out vec4 col;\n"
    "\n"
    "void main()\n"
    "{\n"
    "\tcol = texture(s0, frag_tex);\n"
    "\tcol *= frag_col;\n"
    "}\n";

static const char s_mipmap_cs_glsl[] =
    "#version 430\n"
    "\n"
    "layout(binding = 0) uniform sampler2D src;\n"
    "layout(binding = 1) uniform writeonly image2D dest;\n"
    "\n"
    "layout(location = 0) uniform int src_level;\n"
    "\n"
    "layout(local_size_x = 8, local_size_y = 8) in;\n"
    "\n"
    "vec4 reduce(ivec2 location)\n"
    "{\n"
    "\tvec4 v0 = texelFetch(src, location + ivec2(0, 0), src_level);\n"
    "\tvec4 v1 = texelFetch(src, location + ivec2(0, 1), src_level);\n"
    "\tvec4 v2 = texelFetch(src, location + ivec2(1, 0), src_level);\n"
    "\tvec4 v3 = texelFetch(src, location + ivec2(1, 1), src_level);\n"
    "\treturn (v0 + v1 + v2 + v3) * 0.25;\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "\tvec4 v = reduce(ivec2(gl_GlobalInvocationID.xy * 2));\n"
    "\timageStore(dest, ivec2(gl_GlobalInvocationID.xy), v);\n"
    "}\n";

static const char s_license_reshade[] = "ReShade - BSD-3-Clause License\nCopyright (c) 2014 Patrick Mours";
static const char s_license_glad[] = "GLAD - Public Domain";
static const char s_license_imgui[] = "Dear ImGui - MIT License\nCopyright (c) 2014-2025 Omar Cornut";
static const char s_license_stb[] = "stb - Public Domain / MIT License";
static const char s_license_utfcpp[] = "UTF-CPP - MIT License";
static const char s_license_empty[] = "";

reshade::resources::data_resource reshade::resources::load_data_resource(unsigned short id)
{
	data_resource result = {};

	switch (id)
	{
	case IDR_IMGUI_VS_GLSL:
		result.data = s_imgui_vs_glsl;
		result.data_size = sizeof(s_imgui_vs_glsl) - 1;
		break;
	case IDR_IMGUI_PS_GLSL:
		result.data = s_imgui_ps_glsl;
		result.data_size = sizeof(s_imgui_ps_glsl) - 1;
		break;
	case IDR_MIPMAP_CS_GLSL:
		result.data = s_mipmap_cs_glsl;
		result.data_size = sizeof(s_mipmap_cs_glsl) - 1;
		break;
	case IDR_COPY_PS:
	case IDR_FULLSCREEN_VS:
	case IDR_IMGUI_PS_3_0:
	case IDR_IMGUI_PS_4_0:
	case IDR_IMGUI_PS_SPIRV:
	case IDR_IMGUI_VS_3_0:
	case IDR_IMGUI_VS_4_0:
	case IDR_IMGUI_VS_SPIRV:
	case IDR_MIPMAP_CS:
	case IDR_LICENSE_GLAD:
	case IDR_LICENSE_IMGUI:
	case IDR_LICENSE_MINHOOK:
	case IDR_LICENSE_OPENVR:
	case IDR_LICENSE_OPENXR:
	case IDR_LICENSE_RESHADE:
	case IDR_LICENSE_SPIRV:
	case IDR_LICENSE_STB:
	case IDR_LICENSE_UTFCPP:
	case IDR_LICENSE_VMA:
	case IDR_LICENSE_S_JXL:
		result.data = s_license_empty;
		result.data_size = 0;
		break;
	default:
		result.data = s_license_empty;
		result.data_size = 0;
		break;
	}

	return result;
}

#if RESHADE_LOCALIZATION
std::string reshade::resources::load_string(unsigned short id)
{
	return std::string();
}

std::string reshade::resources::get_current_language()
{
	return "en-US";
}

std::string reshade::resources::set_current_language(const std::string &language)
{
	return language;
}

std::vector<std::string> reshade::resources::get_languages()
{
	return { "en-US" };
}
#endif
