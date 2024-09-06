#include <iostream>
#include <Windows.h>
#include <conio.h>
#include <thread>
#include <atomic>
#include "interception.h"

// The libs are for both debug and release configurations, because they contain debug symbols within them.
#pragma comment(lib, "interception.lib")

namespace Globals
{
	InterceptionContext context{ NULL };
}

void send_key(InterceptionContext context, InterceptionDevice device, unsigned short scanCode, bool keyUp = false)
{
	InterceptionKeyStroke stroke;
	stroke.code = scanCode;
	stroke.state = keyUp ? INTERCEPTION_KEY_UP : INTERCEPTION_KEY_DOWN;

	interception_send(context, device, (InterceptionStroke*)&stroke, 1);
}

void send_key_loop(InterceptionContext context, InterceptionDevice device, UINT scanCode, std::atomic<bool>& running) {
	while (running) {
		send_key(context, device, scanCode);
		Sleep(200);
		send_key(context, device, scanCode, true);
		Sleep(50);
	}
}

void process_key_event(InterceptionKeyStroke& stroke) {
	std::cout << "Key event: code = " << stroke.code
		<< ", state = " << stroke.state << std::endl;
}

BOOL WINAPI ConsoleHandler(DWORD dwCtrlType) {
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

int main()
{
	// This variable is used to control the multi-threaded loop
	std::atomic<bool> running{ true };


	// Set Console Control Handler, so we can handle all sorts of console closing cases

	if (!SetConsoleCtrlHandler(ConsoleHandler, true))
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


	// Convert Virtual Keycodes into ScanCodes that interception can read

	UINT scanCodeA = MapVirtualKey(0x41, MAPVK_VK_TO_VSC);
	UINT scanCodeEnd = MapVirtualKey(VK_END, MAPVK_VK_TO_VSC);


	// Run the key loop in another thread, so as to not interrupt the thread that is checking for scanCodeEnd

	std::thread keyThread(send_key_loop, Globals::context, device, scanCodeA, std::ref(running)); // The std::ref() function ensures that running is passed by reference to the new thread, as std::atomic<bool> cannot be copied. Without std::ref(), std::thread will attempt to copy the arguments leading to errors.


	// Wait for End key to be pressed before terminating (non keyboard blocking)

	InterceptionStroke stroke;
	int receivedKeys;
	while (running) {
		receivedKeys = interception_receive(Globals::context, device, &stroke, 1);

		if (receivedKeys > 0) {
			InterceptionKeyStroke& keyStroke = *(InterceptionKeyStroke*)&stroke;

			// Output pressed keys to the console along with their state (whether the key is down or up)
			process_key_event(keyStroke);

			// Terminate the loop and exit the application if the user pressed the End button
			if (keyStroke.code == scanCodeEnd) {
				running = false;
				break;
			}

			// could even make it so that every key i press is modified into the A key
			// keyStroke.code = scanCodeA

			// Continue sending the stroke to the OS
			interception_send(Globals::context, device, &stroke, 1);
		}

		Sleep(10); // Prevent high CPU usage
	}


	// Inform the user that application is exiting

	std::cout << "Exiting application...\n";


	// Join the two threads back into one

	keyThread.join();


	// Destroy the context

	if (Globals::context != NULL) interception_destroy_context(Globals::context);


	// Wait for user to confirm

	std::cout << "Press any key to continue...\n";
	static_cast<void>(_getch());  // cast to void to tell the compiler to shut up

	return EXIT_SUCCESS;
}