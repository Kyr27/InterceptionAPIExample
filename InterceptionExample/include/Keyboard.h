#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <map>
#include <atomic>
#include "interception.h"

namespace Keyboard
{
	void send_key(InterceptionContext context, InterceptionDevice device, unsigned short scancode, bool key_up = false);

	void send_key_loop(InterceptionContext context, InterceptionDevice device, UINT scancode, std::atomic<bool>& running);

	void process_key_event(InterceptionKeyStroke& stroke);

	void get_key_scancodes(std::map<int, UINT>& key_scancodes);

}