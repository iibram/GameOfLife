#ifndef KEY_INPUT_H
#define KEY_INPUT_H

#include <iostream>

// Platform-specific input multiplexing
#if defined(_WIN32) || defined(_WIN64)
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#endif

/**
 * @namespace KeyInput
 * @brief Provides a lightweight, cross-platform, non-blocking keyboard input interface.
 *
 * This utility abstracts operating system differences between Windows (Console I/O) and POSIX/Linux (termios/select)
 * to capture raw keyboard events without stalling the main execution thread. Ideal for real-time simulations and game loops.
 *
 * @author Ibrahim Ibram
 * @date May 2026
 */
namespace KeyInput {

	/**
	 * @enum class Key
	 * @brief Strongly-typed representation of supported keyboard inputs mapped to standard ASCII/Virtual codes.
	 */
	enum class Key
	{
		None = -1,
		Esc = 27,
		Space = 32,
		Tab = 9,
		Enter = 13,			// Standardized internally to Carriage Return
		W = 119,			// Lowercase ASCII 'w'
		A = 97,				// Lowercase ASCII 'a'
		S = 115,			// Lowercase ASCII 's'
		D = 100,			// Lowercase ASCII 'd'

		// Die glorreichen Vier – für die Zukunft direkt alle vorbereitet
		Up = 1001,
		Down = 1002,
		Left = 1003,
		Right = 1004
	};

	/**
	 * @brief Queries the OS input buffer for a pending keystroke without blocking the calling thread.
	 * @return The strongly-typed Key enum matching the pressed key, or Key::None if no input is available.
	 * * @note On Linux/POSIX systems, this function temporarily disables canonical mode and echo
	 * to intercept raw keystrokes immediately, restoring the original terminal state before returning.
	 */
	inline Key getPressedKey()
	{
		#if defined(_WIN32) || defined(_WIN64)
		// --------------------------- WINDOWS implementation (using conio.h APIs) ---------------------------
		if (_kbhit())
		{
			int rawCode = _getch();

			// WINDOWS ARROW-KEYS: if 0 or 224 appears, the actual signal follows IMMEDIATELY
			if (rawCode == 0 || rawCode == 224)
			{
				int extendedCode = _getch();	// fetches the second byte without blocking
				switch (extendedCode)
				{
					case 72: return Key::Up;
					case 80: return Key::Down;
					case 75: return Key::Left;
					case 77: return Key::Right;
					default: return Key::None;	// or unknown key
				}
			}

			// standardize platform-specific Enter key variations (\r vs \n)
			if (rawCode == 13 || rawCode == 10) return Key::Enter;

			return static_cast<Key>(rawCode);
		}
		return Key::None;
		//----------------------------------------------------------------------------------------------------

		#else
		// ----------------- POSIX/Linux implementation (using termios and I/O multiplexing) -----------------
		struct termios oldt, newt;
		int rawCode = -1;

		// Fetch current terminal attributes
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;

		// Disable canonical mode (line buffering) and local echo
		newt.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);

		// Set up non-blocking synchronous I/O multiplexing via select()
		struct timeval tv = { 0, 0 };			// Immediate timeout (polling)
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);

		// Monitor standard input for readability
		if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0)
		{
			rawCode = getchar();

			// LINUX ARROW-KEYS: ANSI Escape-Sequenz abfangen (z.B. Esc + '[' + 'A')
			if (rawCode == 27)					// Escape gedrückt ODER Anfang einer Sequenz
			{
				// Wir schauen per select, ob SOFORT weitere Bytes im Puffer liegen
				// Wenn ja, ist es eine Pfeiltaste, wenn nein, war es echtes "Esc"
				if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0)
				{
					int nextByte1 = getchar();	// Sollte '[' (91) sein
					int nextByte2 = getchar();	// Sollte 'A', 'B', 'C' oder 'D' sein

					if (nextByte1 == 91)
					{
						switch (nextByte2)
						{
							case 65: return Key::Up;
							case 66: return Key::Down;
							case 68: return Key::Left;
							case 67: return Key::Right;
						}
					}
				}
				return Key::Esc; // Keine weiteren Bytes? Dann war es die echte Esc-Taste!
			}
		}

		// Restore original terminal configurations to prevent console corruption
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

		// Normalize state and cross-platform key codes
		if (rawCode == 10 || rawCode == 13) return Key::Enter;
		if (rawCode == -1) return Key::None;

		return static_cast<Key>(rawCode);
		//----------------------------------------------------------------------------------------------------
		#endif
	}

} // namespace KeyInput

#endif // KEY_INPUT_H
