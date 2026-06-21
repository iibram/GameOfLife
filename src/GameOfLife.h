#pragma once
#include "KeyInput.h"		// includes <iostream> already
#include <condition_variable>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>


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
 * @brief
 */
enum class Figure : uint8_t
{
	// A Randomly generated board where each cell starts as "alive" with a certain probability (default 20%).
	RANDOM,

	// A Glider that starts at the top left, flies perfectly diagonally to the bottom right
	GLIDER,

	// A huge, beautiful oscillator (period 3). Requires about 15x15 space.
	PULSAR,

	// A fascinating oscillator that only repeats after 15 generations.
	// Basically a line of 10 consecutive live cells, horizontally centered.
	PENTADECATHLON,

	// The absolute crown jewel. The first known "weapon" that spawns infinite gliders
	// @note Requires an area of ​​at least 38x11, otherwise it will shoot itself down.
	GOSPER_GLIDER_GUN
};

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
	GameOfLife(uint8_t rows, uint8_t cols, Figure figure, uint8_t percent);
	GameOfLife(uint8_t rows, uint8_t cols, Figure figure);
	GameOfLife(uint8_t rows, uint8_t cols, uint8_t percent = 20);
	~GameOfLife();

private:
	std::stringstream ss;					// reusable `std::stringstream` for a single `std::cout` per generation

	std::mutex workerMutex;					// mutual exclusion lock (mutex) for worker synchronization
	std::condition_variable cvStart;		// signals the worker: "calculate!"
	std::condition_variable cvDone;			// signals the main thread: "done!"
	std::thread workerThread;    			// persistent worker thread for nextBoard processing

	std::string upperBorder;				// the top border as a `std::string` template
	std::string lowerBorder;				// the bottom border as a `std::string` template
	std::vector<uint8_t> currBoard;			// currently displayed board
	std::vector<uint8_t> nextBoard;			// the board of the next generation already calculated in the background
	std::vector<uint16_t> lut_U;			// worker LUT up
	std::vector<uint16_t> lut_D;			// worker LUT down
	std::vector<uint8_t> lut_L;				// worker LUT left
	std::vector<uint8_t> lut_R;				// worker LUT right
	uint32_t currGen = 1;					// number of the current generation
	uint16_t updateTime = 75;				// update time of the main loop in milliseconds (default: 75)
	uint16_t accuTime = 0;					// update Time accumulator
	uint16_t maxSize;						// max. number of elements in the 1D vector
	uint8_t rows;							// number of rows on the board
	uint8_t cols;							// number of columns on the board
	uint8_t percent = 20;					// the percentage probability of each grid cell starting as "alive" (default: 20)
	Figure figure = Figure::RANDOM;
	bool isRunning = false;					// state: is game running?
	bool isPaused = false;					// state: is game paused?

	bool workerReady = false;				// protection against spurious wakeups
	bool workerDone = true;					// status for the main thread
	bool workerRunning = true;				// controls the lifecycle of the worker thread

	void initWorkerLUTs();
	void initWorkerThread();
	void generateRandomBoard();
	void calcNextBoard();
	void setHorizontalBorders();
	void start_loop();
	bool processInput();
	void swapBoards();
	void drawBoard();
	void ss_out_n_flush();
};
