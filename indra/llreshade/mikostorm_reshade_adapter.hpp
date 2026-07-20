/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * MikoStorm ReShade Adapter
 * Bridges MikoStorm's OpenGL context to ReShade's effect runtime.
 */

#pragma once

#include <Windows.h>
#include <cstdint>

class MikoStormReShadeAdapter
{
public:
    MikoStormReShadeAdapter() = default;
    ~MikoStormReShadeAdapter();

    bool init(HWND hwnd, HDC hdc, HGLRC hglrc);
    void shutdown();

    void on_present();
    void on_resize(unsigned int width, unsigned int height);

    bool is_initialized() const { return mInitialized; }

private:
    bool mInitialized = false;
    HWND mHwnd = nullptr;
    void *mDevice = nullptr;
    void *mSwapchain = nullptr;

    unsigned int mLastWidth = 0;
    unsigned int mLastHeight = 0;
};

MikoStormReShadeAdapter *getReShadeAdapter();
void initReShadeAdapter(HWND hwnd, HDC hdc, HGLRC hglrc);
