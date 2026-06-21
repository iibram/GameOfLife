#ifndef KEY_INPUT_H
#define KEY_INPUT_H

#include <iostream>
#include <cstdint>

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
namespace KeyInput
{
	/**
	 * @brief Strongly-typed representation of supported keyboard inputs mapped to standard ASCII/Virtual codes.
	 */
	enum Key : int16_t
	{
		NONE = -1,

		ESC = 27, SPACE = 32, TAB = 9, ENTER = 13,

		W = 119, A = 97, S = 115, D = 100,

		UP = 1001, DOWN = 1002, LEFT = 1003, RIGHT = 1004
	};

	#if defined(_WIN32) || defined(_WIN64)
	// ======================================================================================================================
	// WINDOWS IMPLEMENTATION (using conio.h APIs)
	// ======================================================================================================================

	// Die Dummies für Windows (tun einfach nichts)
	inline void initTerminal() {}
	inline void restoreTerminal() {}

	/**
	 * @brief Queries the OS input buffer for a pending keystroke without blocking the calling thread.
	 * @return The strongly-typed Key enum matching the pressed key, or Key::None if no input is available
	 * @note On Linux/POSIX systems, this function temporarily disables canonical mode and echo to intercept
	 * raw keystrokes immediately, restoring the original terminal state before returning.
	 */
	inline Key getPressedKey()
	{
		if (_kbhit())
		{
			int rawCode = _getch();

			// WINDOWS ARROW-KEYS: if 0 or 224 appears, the actual signal follows IMMEDIATELY
			if (rawCode == 0 || rawCode == 224)
			{
				int extendedCode = _getch();						// fetches the second byte without blocking
				switch (extendedCode)
				{
					case 72: return Key::UP;
					case 80: return Key::DOWN;
					case 75: return Key::LEFT;
					case 77: return Key::RIGHT;
					default: return Key::NONE;						// or unknown key
				}
			}

			// standardize platform-specific Enter key variations (\r vs \n)
			if (rawCode == 13 || rawCode == 10) return Key::ENTER;

			return static_cast<Key>(rawCode);
		}
		return Key::NONE;
	}
	// ======================================================================================================================

	#else
	// ======================================================================================================================
	// LINUX / POSIX IMPLEMENTATION (using termios and I/O multiplexing)
	// ======================================================================================================================

	inline struct termios oldt, newt;

	// Called once when the game starts
	inline void initTerminal()
	{
		tcgetattr(STDIN_FILENO, &oldt);								// Fetch current terminal attributes
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO);							// Disable canonical mode (line buffering) and local echo
		newt.c_cc[VMIN] = 0;										// Set to non-blocking mode
		newt.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	}

	// Called once when the game ends
	inline void restoreTerminal()
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);					// Restore original terminal configurations to prevent console corruption
	}

	inline Key getPressedKey()
	{
		// Since the terminal is permanently in raw mode, a simple select + read suffice
		struct timeval tv = { 0, 0 };								// Immediate timeout (polling)
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);

		if (select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv) > 0)
		{
			unsigned char ch;
			if (read(STDIN_FILENO, &ch, 1) > 0)
			{
				if (ch == 27)										// Escape-Sequenz
				{
					unsigned char seq[2];
					// Since the bytes are stored atomically in the buffer, read directly
					int bytesRead = 0;
					bytesRead += read(STDIN_FILENO, &seq[0], 1);
					bytesRead += read(STDIN_FILENO, &seq[1], 1);

					if (bytesRead == 2 && seq[0] == '[')
					{
						// After successfully reading the arrow key, we flush away
						// any remaining auto-repeat events while in raw mode
						tcflush(STDIN_FILENO, TCIFLUSH);
						switch (seq[1])
						{
							case 'A': return Key::UP;
							case 'B': return Key::DOWN;
							case 'C': return Key::RIGHT;
							case 'D': return Key::LEFT;
						}
					}
					return Key::ESC;								// No following bytes? Genuine ESC.
				}
				if (ch == 10 || ch == 13) return Key::ENTER;
				return static_cast<Key>(ch);
			}
		}
		return Key::NONE;
	}
	// ======================================================================================================================
	#endif

} // namespace KeyInput

#endif // KEY_INPUT_H
