#pragma once
#include "KeyInput.h"	// includes <iostream> already
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <future>		// for std::async


constexpr std::string_view CURSOR_POS_0 = "\033[H";			// ANSI code: move cursor to position 0
constexpr std::string_view CLEAR_DISP = "\033[2J";			// ANSI code: clear console
constexpr std::string_view HIDE_CURSOR = "\033[?25l";		// ANSI code: hide cursor
constexpr std::string_view SHOW_CURSOR = "\033[?25h";		// ANSI code: show cursor
constexpr std::string_view LIVING_CELL = "\U0001F7E9";		// Symbol for a living cell (Unicode: 🟩)
constexpr std::string_view DEAD_CELL = "  ";				// Symbol for dead cell (2 spaces)
constexpr std::string_view LINE_H = "\U00002501";			// Unicode: horizontal line 	(━)
constexpr std::string_view LINE_V = "\U00002503";			// Unicode: vertical line		(┃)
constexpr std::string_view CORNER_TL = "\U0000250F";		// Unicode: top-left corner		(┏)
constexpr std::string_view CORNER_TR = "\U00002513";		// Unicode: top-right corner	(┓)
constexpr std::string_view CORNER_BL = "\U00002517";		// Unicode: bottom-left corner	(┗)
constexpr std::string_view CORNER_BR = "\U0000251B";		// Unicode: bottom-right corner (┛)


/**
 * @brief Implements the well-known CS game "Conway's Game of Life" in the console using a highly efficient implementation.
 * The game board wraps around in all directions, and the game can be started with various parameters. Also the game is
 * controllable by key inputs such as ESC (to end the game), SPACE (to pause/resume the game) and UP/DOWN to control the
 * game speed (by manipulating the sleep time of the main thread) during the game runs.
 *
 * @author Ibrahim Ibram
 * @date April 2026
 */
class GameOfLife
{
public:
	GameOfLife(uint8_t rows, uint8_t cols);
	~GameOfLife();

	void setRandomness(uint8_t percent);
	void start_loop(uint16_t sleepTime);
	void start_loop();

private:
	std::stringstream ss;					// reusable `std::stringstream` for a single `std::cout` per generation
	std::string upperBorder;				// the top border as a `std::string` template
	std::string lowerBorder;				// the bottom border as a `std::string` template
	std::vector<uint8_t> currBoard;			// currently displayed board
	std::vector<uint8_t> nextBoard;			// the board of the next generation already calculated in the background
	std::future<void> worker;				// worker that calculates each nextBoard in the background
	uint32_t currGen = 1;					// number of the current generation
	uint16_t updateTime = 128;				// main thread sleep time in milliseconds (default: 128)
	uint16_t accuTime = 0;					// update Time accumulator
	uint16_t maxSize;						// max. number of elements in the 1D vector
	uint8_t rows;							// number of rows on the board
	uint8_t cols;							// number of columns on the board
	uint8_t percent = 20;					// the percentage probability of each grid cell starting as "alive" (default: 20)
	uint8_t pauseTime = 66;					// main thread sleep time in milliseconds, when gasme is paused (default: 66)
	bool isRunning = false;					// state: is game running?
	bool isPaused = false;					// state: is game paused?

	void generateRandomBoard();
	void init_borders();
	bool processInput();
	void swapBoards();
	void drawBoard();
	void calcNextBoard();
	void ss_out_n_flush();
};
