/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 *
 * MikoStorm: Simplified input registration without hook_manager dependency.
 * ReShade overlay input is handled through MikoStorm's own input system.
 * The handle_window_message function is not implemented since we don't hook
 * the window message pump. The overlay will use GetAsyncKeyState instead.
 */

#include "input.hpp"
#include "dll_log.hpp"
#include <shared_mutex>
#include <unordered_map>
#include <Windows.h>

static std::shared_mutex s_windows_mutex;
static std::unordered_map<HWND, std::weak_ptr<reshade::input>> s_windows;

std::shared_ptr<reshade::input> reshade::input::register_window(window_handle window)
{
	assert(window != nullptr);

	DWORD process_id = 0;
	GetWindowThreadProcessId(static_cast<HWND>(window), &process_id);
	if (process_id != GetCurrentProcessId())
	{
		reshade::log::message(reshade::log::level::warning, "Cannot capture input for window %p created by a different process (%lu).", window, process_id);
		return nullptr;
	}

	const std::unique_lock<std::shared_mutex> lock(s_windows_mutex);

	const auto insert = s_windows.emplace(static_cast<HWND>(window), std::weak_ptr<input>());

	if (insert.second || insert.first->second.expired())
	{
		reshade::log::message(reshade::log::level::debug, "Starting input capture for window %p.", window);

		const auto instance = std::make_shared<input>(window);
		insert.first->second = instance;

		return instance;
	}
	else
	{
		return insert.first->second.lock();
	}
}

void reshade::input::register_window_with_raw_input(window_handle, bool, bool)
{
}

bool reshade::input::handle_window_message(const void *)
{
	return false;
}

void reshade::input::block_mouse_input(bool enable)
{
	// No-op: MikoStorm handles its own input blocking
}

void reshade::input::block_keyboard_input(bool enable)
{
}

void reshade::input::block_mouse_cursor_warping(bool enable)
{
}

bool reshade::input::is_blocking_any_mouse_input(window_handle)
{
	return false;
}

bool reshade::input::is_blocking_any_keyboard_input(window_handle)
{
	return false;
}

bool reshade::input::is_blocking_any_mouse_cursor_warping()
{
	return false;
}

void reshade::input::max_mouse_position(unsigned int position[2]) const
{
	RECT rect = {};
	GetWindowRect(static_cast<HWND>(_window), &rect);
	position[0] = static_cast<unsigned int>(rect.right - rect.left);
	position[1] = static_cast<unsigned int>(rect.bottom - rect.top);
}

bool reshade::input::is_keyboard_layout_german()
{
	return LOBYTE(GetKeyboardLayout(0)) == LANG_GERMAN;
}

void reshade::input::next_frame()
{
	_frame_count++;

	std::copy_n(_keys, 256, _last_keys);

	for (uint8_t &state : _keys)
		state &= ~0x08;

	_text_input.clear();
	_mouse_wheel_delta = 0;
	_last_mouse_position[0] = _mouse_position[0];
	_last_mouse_position[1] = _mouse_position[1];
}

std::shared_ptr<reshade::input_gamepad> reshade::input_gamepad::load()
{
	return nullptr;
}

void reshade::input_gamepad::next_frame()
{
}
