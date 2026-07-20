/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * MikoStorm ReShade Adapter - Bridges MikoStorm's OpenGL to ReShade runtime.
 */

#include "mikostorm_reshade_adapter.hpp"
#include "opengl/opengl_impl_device.hpp"
#include "opengl/opengl_impl_device_context.hpp"
#include "opengl/opengl_impl_swapchain.hpp"
#include "runtime.hpp"
#include "runtime_manager.hpp"
#include "ini_file.hpp"
#include "dll_log.hpp"
#include "version.h"

#include <glad/gl.h>
#include <glad/wgl.h>

#include <filesystem>
#include <ShlObj.h>

std::filesystem::path g_reshade_dll_path;
std::filesystem::path g_reshade_base_path;
std::filesystem::path g_target_executable_path;
HMODULE g_module_handle = nullptr;
HANDLE g_exit_event = nullptr;

struct wgl_device : public reshade::opengl::device_impl, public reshade::opengl::device_context_impl
{
    wgl_device(HDC initial_hdc, HGLRC hglrc, const GladGLContext &dispatch_table, bool compatibility_context) :
        device_impl(initial_hdc, hglrc, dispatch_table, compatibility_context),
        device_context_impl(this, hglrc)
    {
    }
};

struct MikoStormReShadeAdapterImpl
{
    wgl_device *device = nullptr;
    reshade::opengl::swapchain_impl *swapchain = nullptr;
    bool runtime_created = false;
};

static MikoStormReShadeAdapter *s_adapter = nullptr;
static MikoStormReShadeAdapterImpl s_impl = {};

MikoStormReShadeAdapter *getReShadeAdapter()
{
    return s_adapter;
}

static GLADapiproc glad_load_gl_proc(const char *name)
{
    GLADapiproc proc_address = reinterpret_cast<GLADapiproc>(wglGetProcAddress(name));
    if (proc_address == nullptr)
    {
        static const HMODULE opengl_module = GetModuleHandleW(L"opengl32.dll");
        if (opengl_module != nullptr)
            proc_address = reinterpret_cast<GLADapiproc>(GetProcAddress(opengl_module, name));
    }
    return proc_address;
}

void MikoStormReShadeAdapter::shutdown()
{
    if (s_impl.runtime_created && s_impl.swapchain)
    {
        reshade::reset_effect_runtime(s_impl.swapchain);
        s_impl.runtime_created = false;
    }

    if (s_impl.swapchain)
    {
        reshade::destroy_effect_runtime(s_impl.swapchain);
        delete s_impl.swapchain;
        s_impl.swapchain = nullptr;
    }

    if (s_impl.device)
    {
        delete s_impl.device;
        s_impl.device = nullptr;
    }

    mDevice = nullptr;
    mSwapchain = nullptr;
    mInitialized = false;
    mLastWidth = 0;
    mLastHeight = 0;
}

MikoStormReShadeAdapter::~MikoStormReShadeAdapter()
{
    shutdown();
    if (s_adapter == this)
        s_adapter = nullptr;
}

bool MikoStormReShadeAdapter::init(HWND hwnd, HDC hdc, HGLRC hglrc)
{
    if (mInitialized)
        return true;

    g_module_handle = GetModuleHandleW(nullptr);

    WCHAR exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    g_target_executable_path = exe_path;
    g_reshade_dll_path = g_target_executable_path;

    WCHAR appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata)))
        g_reshade_base_path = std::filesystem::path(appdata) / L"MikoStorm" / L"ReShade";
    else
        g_reshade_base_path = g_target_executable_path.parent_path();

    std::error_code ec;
    std::filesystem::create_directories(g_reshade_base_path, ec);

    reshade::log::open_log_file(g_reshade_base_path / L"MikoStorm_ReShade.log", ec);

    reshade::log::message(reshade::log::level::info,
        "Initializing MikoStorm ReShade adapter (version '" VERSION_STRING_FILE "') ...");

    GladGLContext gl_context = {};
    gladLoadWGL(hdc, glad_load_gl_proc);
    gladLoadGLContext(&gl_context, glad_load_gl_proc);

    if (!gl_context.VERSION_4_3)
    {
        reshade::log::message(reshade::log::level::error,
            "OpenGL 4.3 is required but not available. ReShade effects will not load.");
        return false;
    }

    {
        GLint major = 0, minor = 0;
        gl_context.GetIntegerv(GL_MAJOR_VERSION, &major);
        gl_context.GetIntegerv(GL_MINOR_VERSION, &minor);
        reshade::log::message(reshade::log::level::info,
            "OpenGL %d.%d loaded. Renderer: %s",
            major, minor,
            reinterpret_cast<const char *>(gl_context.GetString(GL_RENDERER)));
    }

    s_impl.device = new wgl_device(hdc, hglrc, gl_context, false);
    s_impl.swapchain = new reshade::opengl::swapchain_impl(s_impl.device, hdc);

    mHwnd = hwnd;
    mDevice = s_impl.device;
    mSwapchain = s_impl.swapchain;
    mInitialized = true;

    reshade::log::message(reshade::log::level::info,
        "MikoStorm ReShade adapter initialized successfully.");

    return true;
}

void MikoStormReShadeAdapter::on_resize(unsigned int width, unsigned int height)
{
    if (!mInitialized || !s_impl.swapchain)
        return;

    if (width == mLastWidth && height == mLastHeight)
        return;

    if (width == 0 || height == 0)
    {
        if (s_impl.runtime_created)
        {
            reshade::reset_effect_runtime(s_impl.swapchain);
            s_impl.runtime_created = false;
        }
        mLastWidth = 0;
        mLastHeight = 0;
        return;
    }

    reshade::log::message(reshade::log::level::info,
        "Resizing ReShade runtime to %ux%u ...", width, height);

    if (s_impl.runtime_created)
    {
        reshade::reset_effect_runtime(s_impl.swapchain);
        s_impl.runtime_created = false;
    }

    mLastWidth = width;
    mLastHeight = height;

    reshade::create_effect_runtime(s_impl.swapchain, s_impl.device);
    reshade::init_effect_runtime(s_impl.swapchain);
    s_impl.runtime_created = true;
}

void MikoStormReShadeAdapter::on_present()
{
    if (!mInitialized || !s_impl.swapchain)
        return;

    RECT window_rect = {};
    GetClientRect(mHwnd, &window_rect);
    const unsigned int width = static_cast<unsigned int>(window_rect.right);
    const unsigned int height = static_cast<unsigned int>(window_rect.bottom);

    if (width == 0 || height == 0)
        return;

    if (width != mLastWidth || height != mLastHeight)
        on_resize(width, height);

    if (!s_impl.runtime_created)
        return;

    reshade::present_effect_runtime(s_impl.swapchain);
}

void initReShadeAdapter(HWND hwnd, HDC hdc, HGLRC hglrc)
{
    if (s_adapter == nullptr)
        s_adapter = new MikoStormReShadeAdapter();

    s_adapter->init(hwnd, hdc, hglrc);
}
