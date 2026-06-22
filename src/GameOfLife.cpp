#include "GameOfLife.h"
#include <format>


/**
 * @brief Custom constructor: Reserves space for the `std::stringstream` and sets the randomness of the board.
 * Also invokes the initializations of the random board and the worker thread.
 * @param rows number of rows on the board
 * @param cols number of cols on the board
 * @param figure enum class Figure {RANDOM, GLIDER, Pulsar,...}
 * @param percent percentage of the randomness as decimal (0 – 100)
 */
GameOfLife::GameOfLife(uint8_t rows, uint8_t cols, Figure figure, uint8_t percent)
	: currBoard(static_cast<size_t>(rows)* cols, 0)
	, nextBoard(static_cast<size_t>(rows)* cols, 0)
	, maxSize(static_cast<uint16_t>(currBoard.size()))
	, rows(rows), cols(cols)
	, figure(figure)
{
	Platform::initTerminal();

	uint32_t assumedSize = maxSize * 4 + rows;		// An emoji square takes up approx. 4 bytes in UTF-8, plus any newlines
	assumedSize += ((cols << 1) + (rows << 1));		// + borders

	ss.rdbuf()->str().reserve(assumedSize);			// reserving enough space for the stringsstream buffer (board + borders)

	if (percent < 101)								// setting the probability each grid cell starts as "alive" (default: 20, affect only RANDOM)
		this->percent = percent;

	initWorkerLUTs();
	initWorkerThread();
	generateRandomBoard();
	calcNextBoard();								// get the worker warmed up for the very first run!
	setHorizontalBorders();
	start_loop();
}

/**
 * @brief Custom constructor 1: Invokes constructor 0 with accordingly set values
 * @param rows number of rows on the board
 * @param cols number of cols on the board
 * @param percent percentage of the randomness as decimal (0 – 100)
 */
GameOfLife::GameOfLife(uint8_t rows, uint8_t cols, uint8_t percent) :GameOfLife(rows, cols, Figure::RANDOM, percent) {}

/**
 * @brief Custom constructor 2: Invokes constructor 0 with accordingly set values
 * @param rows number of rows on the board
 * @param cols number of cols on the board
 * @param figure enum class Figure {RANDOM, GLIDER, PULSAR,...}
 */
GameOfLife::GameOfLife(uint8_t rows, uint8_t cols, Figure figure) : GameOfLife(rows, cols, figure, 20) {}

/**
 * @brief Custom destructor: Clean termination of the worker thread when game is over (isRunning == false).
 */
GameOfLife::~GameOfLife()
{
	// signal the worker to terminate
	{
		std::lock_guard<std::mutex> lock(workerMutex);
		workerRunning = false;
		workerReady = true;
	}

	// trigger BOTH variables to resolve ANY blockage in the worker or main thread
	cvStart.notify_all();
	cvDone.notify_all();

	// wait until the thread has terminated cleanly
	if (workerThread.joinable())
		workerThread.join();

	Platform::restoreTerminal();
}

void GameOfLife::initWorkerLUTs()
{
	lut_U.resize(rows);
	lut_D.resize(rows);
	lut_L.resize(cols);
	lut_R.resize(cols);

	for (uint8_t r = 0; r < rows; ++r)
	{
		lut_U[r] = (r == 0) ? static_cast<uint16_t>((rows - 1) * cols) : static_cast<uint16_t>((r - 1) * cols);
		lut_D[r] = (r == rows - 1) ? 0 : static_cast<uint16_t>((r + 1) * cols);
	}

	for (uint8_t c = 0; c < cols; ++c)
	{
		lut_L[c] = (c == 0) ? static_cast<uint8_t>(cols - 1) : static_cast<uint8_t>(c - 1);
		lut_R[c] = (c == cols - 1) ? 0 : static_cast<uint8_t>(c + 1);
	}
}

/**
 * @brief Initializes the persistent worker thread and its working environment and job.
 */
void GameOfLife::initWorkerThread()
{
	if (workerThread.joinable()) return;

	workerRunning = true;

	workerThread = std::thread([this]()
	{
		// ------------------------------------------------------ WORKER THREADS AREA ---------------------------------------------------------
		while (workerRunning)
		{
			std::unique_lock<std::mutex> lock(workerMutex);							// protects the worker's lifecycle variables
			cvStart.wait(lock, [this]() { return workerReady || !workerRunning; });	// Worker blocks here until a new job is triggered/shutdown

			// ---------------------------------------------------- WORKER TASK AREA ----------------------------------------------------------
			if (!workerRunning) break;												// Exit loop immediately if thread is shutting down

			uint16_t i = 0;

			// --- THE MAIN ALGORITHM (highly optimized) ---
			for (uint8_t r = 0; r < rows; ++r)
			{
				uint16_t rIdx = static_cast<uint16_t>(r * cols);
				uint16_t rUp = lut_U[r];
				uint16_t rDown = lut_D[r];

				for (uint8_t c = 0; c < cols; ++c)
				{
					uint8_t cLeft = lut_L[c];
					uint8_t cRight = lut_R[c];

					// Pure memory additions in 16-bit space with a single final cast
					int8_t livingNeighbors = static_cast<int8_t>(
						currBoard[rUp + cLeft] + currBoard[rUp + c] + currBoard[rUp + cRight] +
						currBoard[rIdx + cLeft] + currBoard[rIdx + cRight] +
						currBoard[rDown + cLeft] + currBoard[rDown + c] + currBoard[rDown + cRight]
						);

					// ----- THE GAME RULES CAN BE CHANGED HERE (if you know what you are doing!) -----
					uint8_t currState = currBoard[i];
					nextBoard[i] = (livingNeighbors == 3 || (currState == 1 && livingNeighbors == 2)) ? 1 : 0;

					++i;
				}
			}

			// Task finished: Reset flags and notify the main thread
			workerReady = false;
			workerDone = true;
			lock.unlock();			// Release lock explicitly before notifying to prevent priority inversion
			cvDone.notify_one();
		}
	});
}

/**
 * @brief Generates a randomly set board according the setup values
 */
void GameOfLife::generateRandomBoard()
{
	switch (figure)
	{
		case Figure::GLIDER:
		{
			currBoard[(1 * cols) + 2] = 1;
			currBoard[(2 * cols) + 3] = 1;
			currBoard[(3 * cols) + 1] = 1;
			currBoard[(3 * cols) + 2] = 1;
			currBoard[(3 * cols) + 3] = 1;
			break;
		}

		case Figure::PULSAR:
		{
			int z = (rows >> 1) - 6;	// centering
			int s = (cols >> 1) - 6;	// centering

			// horizontal lines of three
			for (int i : {0, 6})
			{
				for (int j : {2, 3, 4, 8, 9, 10})
				{
					currBoard[((z + i) * cols) + (s + j)] = 1;
					currBoard[((z + i + 7) * cols) + (s + j)] = 1;
				}
			}
			// vertical lines of three
			for (int i : {2, 3, 4, 8, 9, 10})
			{
				for (int j : {0, 6})
				{
					currBoard[((z + i) * cols) + (s + j)] = 1;
					currBoard[((z + i) * cols) + (s + j + 7)] = 1;
				}
			}
			break;
		}

		case Figure::PENTADECATHLON:
		{
			int startZeile = (rows >> 1);		// centering
			int startSpalte = (cols >> 1) - 5;	// centering

			for (int i = 0; i < 10; ++i)
				currBoard[(startZeile * cols) + (startSpalte + i)] = 1;

			break;
		}

		case Figure::GOSPER_GLIDER_GUN:
		{
			int z = 2;
			int s = 1;

			// left block
			currBoard[((z + 4) * cols) + (s + 0)] = 1; currBoard[((z + 4) * cols) + (s + 1)] = 1;
			currBoard[((z + 5) * cols) + (s + 0)] = 1; currBoard[((z + 5) * cols) + (s + 1)] = 1;

			// left eye / C-shape
			currBoard[((z + 4) * cols) + (s + 10)] = 1; currBoard[((z + 5) * cols) + (s + 10)] = 1; currBoard[((z + 6) * cols) + (s + 10)] = 1;
			currBoard[((z + 3) * cols) + (s + 11)] = 1; currBoard[((z + 7) * cols) + (s + 11)] = 1;
			currBoard[((z + 2) * cols) + (s + 12)] = 1; currBoard[((z + 8) * cols) + (s + 12)] = 1;
			currBoard[((z + 2) * cols) + (s + 13)] = 1; currBoard[((z + 8) * cols) + (s + 13)] = 1;
			currBoard[((z + 5) * cols) + (s + 14)] = 1;
			currBoard[((z + 3) * cols) + (s + 15)] = 1; currBoard[((z + 7) * cols) + (s + 15)] = 1;
			currBoard[((z + 4) * cols) + (s + 16)] = 1; currBoard[((z + 5) * cols) + (s + 16)] = 1; currBoard[((z + 6) * cols) + (s + 16)] = 1;
			currBoard[((z + 5) * cols) + (s + 17)] = 1;

			// right eye / mushroom shape
			currBoard[((z + 2) * cols) + (s + 20)] = 1; currBoard[((z + 3) * cols) + (s + 20)] = 1; currBoard[((z + 4) * cols) + (s + 20)] = 1;
			currBoard[((z + 2) * cols) + (s + 21)] = 1; currBoard[((z + 3) * cols) + (s + 21)] = 1; currBoard[((z + 4) * cols) + (s + 21)] = 1;
			currBoard[((z + 1) * cols) + (s + 22)] = 1; currBoard[((z + 5) * cols) + (s + 22)] = 1;
			currBoard[((z + 0) * cols) + (s + 24)] = 1; currBoard[((z + 1) * cols) + (s + 24)] = 1;
			currBoard[((z + 5) * cols) + (s + 24)] = 1; currBoard[((z + 6) * cols) + (s + 24)] = 1;

			// right block
			currBoard[((z + 2) * cols) + (s + 34)] = 1; currBoard[((z + 3) * cols) + (s + 34)] = 1;
			currBoard[((z + 2) * cols) + (s + 35)] = 1; currBoard[((z + 3) * cols) + (s + 35)] = 1;
			break;
		}

		case Figure::RANDOM:
		default:
		{
			// generates a true random seed.
			std::random_device rd;
			std::mt19937 gen(rd());

			// clean conversion, e.g. 20 (uint8_t) to 0.2 (double)
			std::bernoulli_distribution dist(static_cast<double>(percent) / 100.0);

			// each cell starts as "alive" with a certain probability (default 20%)
			for (uint16_t i = 0; i < maxSize; ++i)
				currBoard[i] = dist(gen) ? 1 : 0;

			break;
		}
	}
}

/**
 * @brief After a brief initialization, the worker performs all calculations within this function. Based on the game rules
 * and the state of the `currBoard`, it calculates the state of the next generation and writes it to the `nextBoard`.
 */
void GameOfLife::calcNextBoard()
{
	// trigger the worker for the next round
	{
		std::lock_guard<std::mutex> lock(workerMutex);
		workerDone = false;
		workerReady = true;
	}
	cvStart.notify_one(); // wake up the worker!
}

/**
 * @brief Generates the strings for the top and bottom edges of the board once, for direct reuse during every `drawBoard()` update.
 */
void GameOfLife::setHorizontalBorders()
{
	std::string lineSegment;
	// reserve the exact size needed up front
	lineSegment.reserve((cols << 1) * LINE_H.size());

	for (uint8_t c = 0; c < cols; ++c)
	{
		lineSegment.append(LINE_H);
		lineSegment.append(LINE_H);
	}

	upperBorder = std::format("{}{}{}\n", CORNER_TL, lineSegment, CORNER_TR);
	lowerBorder = std::format("{}{}{}\n", CORNER_BL, lineSegment, CORNER_BR);
}

/**
 * @brief Starts the game loop
 */
void GameOfLife::start_loop()
{
	// flush the console buffer, delete all characters, and hide the cursor
	std::cout << std::flush << CLEAR_DISP << HIDE_CURSOR;

	isRunning = true;
	isPaused = false;
	currGen = 1;

	// In deiner main-Schleife:
	auto lastTime = std::chrono::steady_clock::now();
	accuTime = updateTime;

	while (isRunning)
	{
		auto now = std::chrono::steady_clock::now();
		auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
		lastTime = now;

		accuTime += elapsedTime;

		if (processInput())
			drawBoard();				// draws the currBoard with updated texts

		if (accuTime >= updateTime)
		{
			if (!isPaused && isRunning)
			{
				++currGen;

				swapBoards();			// pick up and exchange the finished board
				drawBoard();			// draw the board onto the screen
				calcNextBoard();		// immediately let the worker calculate the NEXT generation!

				accuTime = 0;
			}
		}

		// CPU conserving sleep time (@ OS specific sweet spots)
		std::this_thread::sleep_for(std::chrono::milliseconds(Platform::mainThreadSleepTime));
	}

	// game is over; show the cursor again
	std::cout << SHOW_CURSOR;
}

/**
 * @brief Pushes the full board (+ title bar and all borders) into the `std::stringstream` buffer and by invoking
 * `ss_out_n_flush()` an immediate output of the buffer to the console is forced.
 */
void GameOfLife::drawBoard()
{
	ss << CURSOR_POS_0;
	ss << std::format(" {:^{}} \n", std::format("--- Generation: {:010} ---", currGen), (cols << 1));

	ss << upperBorder;							// ┏━━━ ... ━━━┓
	// draw the full board + the L/R borders
	for (uint16_t r = 0; r < rows; ++r)
	{
		ss << LINE_V;
		for (uint16_t c = 0; c < cols; ++c)
			ss << (currBoard[r * cols + c] == 1 ? LIVING_CELL : DEAD_CELL);
		ss << LINE_V << '\n';
	}
	ss << lowerBorder;							// ┗━━━ ... ━━━┛

	if (isRunning)
		ss << std::format("{0:<{2}}{1:>{2}}", (isPaused ? "[PAUSED]" : "RUNNING"), std::format("update: {:>4} ms", updateTime), (cols + 1));
	else
		ss << std::format(" {:^{}} \n", "------  GAME OVER  ------", (cols << 1));

	ss_out_n_flush();
}

/**
 * @brief Properly performs the swap of the two boards (currBoard <--> nextBoard)
 */
void GameOfLife::swapBoards()
{
	// wait for the worker to finish and pick up the finished board
	std::unique_lock<std::mutex> lock(workerMutex);
	cvDone.wait(lock, [this]() { return workerDone; });

	// pointer swap in O(1)
	std::swap(currBoard, nextBoard);
}

/**
 * @brief Checks whether a control key was pressed and performs the corresponding action
 */
bool GameOfLife::processInput()
{
	Platform::Key key = Platform::getPressedKey();

	if (key != Platform::Key::NONE)
	{
		uint16_t step = (updateTime > 345) ? 10 : 5;

		if (key == Platform::Key::ESC)				// ending the game
			isRunning = false;

		else if (key == Platform::Key::SPACE)		// pausing the game
		{
			isPaused = !isPaused;
			accuTime = 0;
		}

		else if (key == Platform::Key::UP)			// speeding up the framerate
		{
			if (updateTime > step)
				updateTime -= step;
			else
				updateTime = 5;
		}

		else if (key == Platform::Key::DOWN)		// slowing down the framerate
		{
			updateTime += step;
			if (updateTime > 1000) updateTime = 1000;
		}

		return true;
	}

	return false;
}

/**
 * @brief Forces the output of the buffered characters to the console and clears the string stream for reuse
 */
void GameOfLife::ss_out_n_flush()
{
	std::cout << ss.str() << std::flush;
	ss.str(""); ss.clear();
}
