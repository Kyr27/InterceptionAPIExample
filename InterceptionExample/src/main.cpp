#include <iostream>
#include <Windows.h>
#include <conio.h>
#include <thread>
#include <atomic>
#include <map>
#include "interception.h"

// Todo:
// Figure out a way to load the .dll from lib folder directly, so that theres no need to drag the .dll to where the .exe file is
// See if theres a way to move the context global into int main(), and pass it as a parameter to the console_handler function which needs it

// The libs are for both debug and release configurations, because they contain debug symbols within them.
#pragma comment(lib, "interception.lib")

namespace Globals
{
	InterceptionContext context{ NULL };
}

void send_key(InterceptionContext context, InterceptionDevice device, unsigned short scancode, bool key_up = false)
{
	InterceptionKeyStroke stroke;
	stroke.code = scancode;
	stroke.state = key_up ? INTERCEPTION_KEY_UP : INTERCEPTION_KEY_DOWN;

	interception_send(context, device, (InterceptionStroke*)&stroke, 1);
}

void send_key_loop(InterceptionContext context, InterceptionDevice device, UINT scancode, std::atomic<bool>& running) {
	while (running) {
		send_key(context, device, scancode);
		Sleep(200);
		send_key(context, device, scancode, true);
		Sleep(50);
	}
}

void process_key_event(InterceptionKeyStroke& stroke) {
	std::cout << "Key event: code = " << stroke.code
		<< ", state = " << stroke.state << std::endl;
}

BOOL WINAPI console_handler(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (Globals::context != NULL) {
			// Destroy the context if the application is closed via any of the above cases
			interception_destroy_context(Globals::context);
			std::cout << "Context destroyed\n";
		}
		return TRUE;
	default:
		return FALSE;
	}
}

void get_key_scancodes(std::map<int, UINT> &key_scancodes)
{
	for (char key = 'A'; key <= 'Z'; ++key)
	{
		SHORT vk = VkKeyScan(key);

		// Check if we succeeded in mapping the key to a virtual key
		if (!vk)
		{
			std::cout << "Key: " << key << " can not be mapped to a virtual keycode\n";
			continue;
		}

		// Extract the lower byte, which contains the Virtual Keycode (the upper byte is the shift state)
		UINT virtual_key = vk & 0xFF;


		// Store the scan code in the map, using the character as the key
		key_scancodes[static_cast<int>(key)] = MapVirtualKey(virtual_key, MAPVK_VK_TO_VSC);
	}

	// Additionally, add non character keys such as VK_END
	key_scancodes[VK_END] = MapVirtualKey(VK_END, MAPVK_VK_TO_VSC);
}

void run(std::atomic<bool> &running, InterceptionDevice &device, std::map<int, UINT> &key_scancodes)
{
	InterceptionStroke stroke;
	int received_keys;
	while (running) {
		received_keys = interception_receive(Globals::context, device, &stroke, 1);

		if (received_keys > 0) {
			InterceptionKeyStroke& key_stroke = *(InterceptionKeyStroke*)&stroke;

			// Output pressed keys to the console along with their state (whether the key is down or up)
			process_key_event(key_stroke);

			// Terminate the loop and exit the application if the user pressed the End button
			if (key_stroke.code == key_scancodes[VK_END]) {
				running = false;
				break;
			}

			// could even make it so that every key i press is modified into the A key
			// keyStroke.code = key_scancodes['A'];

			// Continue sending the stroke to the OS
			interception_send(Globals::context, device, &stroke, 1);
		}

		Sleep(10); // Prevent high CPU usage
	}
}

int main()
{
	// Holds the scan codes (value) of character keys (key) on a keyboard
	std::map<int, UINT> key_scancodes{};

	// This variable is used to control the multi-threaded loop
	std::atomic<bool> running{ true };


	// Set Console Control Handler, so we can handle all sorts of console closing cases

	if (!SetConsoleCtrlHandler(console_handler, true))
	{
		std::cerr << "Failed to set console control handler\n";
		return EXIT_FAILURE;
	}


	// Initialize interception context

	Globals::context = interception_create_context();
	std::cout << "Context Set\n";


	// Set the event filtering to keyboard, so we listen in on keyboard events

	interception_set_filter(Globals::context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_ALL);
	std::cout << "Filter set\n";


	// Wait for input event from any keyboard device and generate a handle to that device

	InterceptionDevice device = interception_wait(Globals::context);


	// Convert Virtual Keycodes into scancodes that interception can read and store them in key_scancodes

	get_key_scancodes(key_scancodes);


	// run the key loop in another thread, so as to not interrupt the thread that is checking for VK_END
	// The std::ref() around atomic running ensures that running is passed by reference to the new thread, as std::atomic<bool> cannot be copied. Without std::ref(), std::thread will attempt to copy the arguments leading to errors.

	std::thread send_key_thread(send_key_loop, Globals::context, device, key_scancodes['A'], std::ref(running)); 


	// Monitor for VK_END and quit if it pressed

	run(std::ref(running), device, key_scancodes);


	// Inform the user that application is exiting

	std::cout << "Exiting application...\n";


	// Join the two threads back into one

	send_key_thread.join();


	// Destroy the context

	if (Globals::context != NULL) interception_destroy_context(Globals::context);


	// Wait for user to confirm

	std::cout << "Press any key to continue...\n";
	static_cast<void>(_getch());  // cast to void to tell the compiler to shut up

	return EXIT_SUCCESS;
}