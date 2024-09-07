#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <thread>
#include <atomic>
#include <map>
#include "Keyboard.h"
#include "interception.h"

// Todo:
// Figure out a way to load the .dll from lib folder directly, so that theres no need to drag the .dll to where the .exe file is
// See if theres a way to move the context global into int main(), and pass it as a parameter to the console_handler function which needs it

// The libs are for both debug and release configurations, because they contain debug symbols within them.
#pragma comment(lib, "interception.lib")

namespace g
{
	// It has to be a global so i can destroy it from console control handler (which does not allow me to pass parameters)
	static InterceptionContext input_context{ NULL };
}


// Forward Declarations

void run(std::atomic<bool>& running, InterceptionDevice& device, std::map<int, UINT>& key_scancodes);
BOOL WINAPI console_handler(DWORD dwCtrlType);


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

	g::input_context = interception_create_context();
	std::cout << "Context Set\n";


	// Set the event filtering to keyboard, so we listen in on keyboard events

	interception_set_filter(g::input_context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_ALL);
	std::cout << "Filter set\n";


	// Wait for input event from any keyboard device and generate a handle to that device

	InterceptionDevice device = interception_wait(g::input_context);


	// Convert Virtual Keycodes into scancodes that interception can read and store them in key_scancodes

	Keyboard::get_key_scancodes(key_scancodes);


	// Run the main logic

	run(std::ref(running), device, key_scancodes);


	// Inform the user that application is exiting

	std::cout << "Exiting application...\n";


	// Destroy interception context

	if (g::input_context != NULL) interception_destroy_context(g::input_context);


	// Wait for user to confirm

	std::cout << "Press any key to continue...\n";
	static_cast<void>(_getch());  // cast to void to tell the compiler to shut up

	return EXIT_SUCCESS;
}

void run(std::atomic<bool>& running, InterceptionDevice& device, std::map<int, UINT>& key_scancodes)
{
	// run the key loop in another thread, so as to not interrupt the thread that is checking for VK_END
	// The std::ref() around atomic running ensures that running is passed by reference to the new thread, as std::atomic<bool> cannot be copied. Without std::ref(), std::thread will attempt to copy the arguments leading to errors.

	std::thread send_key_thread(Keyboard::send_key_loop, g::input_context, device, key_scancodes['A'], std::ref(running));

	InterceptionStroke stroke;
	int received_keys;
	while (running) {
		received_keys = interception_receive(g::input_context, device, &stroke, 1);

		if (received_keys > 0) {
			InterceptionKeyStroke& key_stroke = *(InterceptionKeyStroke*)&stroke;

			// Output pressed keys to the console along with their state (whether the key is down or up)
			Keyboard::process_key_event(key_stroke);

			// Terminate the loop and exit the application if the user pressed the End button
			if (key_stroke.code == key_scancodes[VK_END]) {
				running = false;
				break;
			}

			// could even make it so that every key i press is modified into the A key
			// keyStroke.code = key_scancodes['A'];

			// Continue sending the stroke to the OS
			interception_send(g::input_context, device, &stroke, 1);
		}

		Sleep(10); // Prevent high CPU usage
	}


	// Join the two threads back into one after VK_END is received

	if (send_key_thread.joinable()) send_key_thread.join();
}

BOOL WINAPI console_handler(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (g::input_context != NULL) {
			// Destroy the context if the application is closed via any of the above cases
			interception_destroy_context(g::input_context);
			std::cout << "Context destroyed\n";
		}
		return TRUE;
	default:
		return FALSE;
	}
}