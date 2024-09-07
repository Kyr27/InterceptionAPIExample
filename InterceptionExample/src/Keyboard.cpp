#include "Keyboard.h"

void Keyboard::send_key(InterceptionContext context, InterceptionDevice device, unsigned short scancode, bool key_up)
{
	InterceptionKeyStroke stroke;
	stroke.code = scancode;
	stroke.state = key_up ? INTERCEPTION_KEY_UP : INTERCEPTION_KEY_DOWN;

	interception_send(context, device, (InterceptionStroke*)&stroke, 1);
}

void Keyboard::send_key_loop(InterceptionContext context, InterceptionDevice device, UINT scancode, std::atomic<bool>& running) {
	while (running) {
		send_key(context, device, scancode);
		Sleep(200);
		send_key(context, device, scancode, true);
		Sleep(50);
	}
}

void Keyboard::process_key_event(InterceptionKeyStroke& stroke) {
	std::cout << "Key event: code = " << stroke.code
		<< ", state = " << stroke.state << std::endl;
}

void Keyboard::get_key_scancodes(std::map<int, UINT>& key_scancodes)
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
