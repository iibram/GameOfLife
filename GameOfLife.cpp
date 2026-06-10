#include "GameOfLife.h"
#include <format>


/**
 * @brief Custom constructor.
 * @param r number of rows on the board
 * @param c number of columns on the board
 */
GameOfLife::GameOfLife(uint8_t rows, uint8_t cols)
	: currBoard(static_cast<size_t>(rows)* cols, 0)
	, nextBoard(static_cast<size_t>(rows)* cols, 0)
	, maxSize(static_cast<uint16_t>(currBoard.size()))
	, rows(rows), cols(cols)
{
	uint32_t assumedSize = maxSize * 4 + rows;		// An emoji square takes up approx. 4 bytes in UTF-8, plus any newlines
	assumedSize += ((cols << 1) + (rows << 1));		// + borders

	ss.rdbuf()->str().reserve(assumedSize);			// reserving enough space for the board + borders for the stringsstream buffer
	std::vector<int> b;
}

/**
 * @brief Default-Destruktor
 */
GameOfLife::~GameOfLife() = default;

/**
 * @brief Sets the percentage probability that each grid cell starts as "alive" (default: 20).
 * @param percent percentage value as a decimal (0 – 100)
 */
void GameOfLife::setRandomness(uint8_t percent)
{
	if (percent < 101)
		this->percent = percent;
	generateRandomBoard();
}

/**
 * @brief Starts the game loop according to the passed parameters
 * @param updateTime main thread sleep time in milliseconds (possible: 1 – 1000)
 */
void GameOfLife::start_loop(uint16_t updateTime)
{
	if (updateTime == 0)
		this->updateTime = 1;
	else if (updateTime > 1000)
		this->updateTime = 1000;
	else
		this->updateTime = updateTime;

	start_loop();
}

/**
 * @brief Starts the game loop with the set or default values
 */
void GameOfLife::start_loop()
{
	// flush the console buffer, delete all characters, and hide the cursor
	std::cout << std::flush << CLEAR_DISP << HIDE_CURSOR;

	init_borders();
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
			drawBoard();				// just draw the board

		if (accuTime >= updateTime)
		{
			if (!isPaused)
			{
				++currGen;

				swapBoards();			// 1. pick up and exchange the finished board
				drawBoard();			// 2. draw the board onto the screen
				calcNextBoard();		// 3. immediately restart the worker for the NEXT generation!

				accuTime -= updateTime;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(5));	// CPU conserving base sleep
	}

	// game is over; show the cursor again
	std::cout << SHOW_CURSOR;
}

/**
 * @brief Generates the strings for the top and bottom edges of the board once, for direct reuse during every update.
 */
void GameOfLife::init_borders()
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
 * @brief
 */
void GameOfLife::generateRandomBoard()
{
	// generates a true random seed.
	std::random_device rd;
	std::mt19937 gen(rd());

	// clean conversion, e.g. 20 (uint8_t) to 0.2 (double)
	std::bernoulli_distribution dist(static_cast<double>(percent) / 100.0);

	// each cell starts as "alive" with a certain probability (default 20%).
	for (uint16_t i = 0; i < maxSize; ++i)
		currBoard[i] = dist(gen) ? 1 : 0;

	calcNextBoard(); // get the worker warmed up for the very first run!
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
	if (worker.valid())						// wait for the worker to finish (99.9% should be so)
		worker.get();						// pick up the finished board and terminate the worker thread properly

	std::swap(currBoard, nextBoard);		// pointer swap in O(1)
}

/**
 * @brief After a brief initialization, the worker performs all calculations within this function. Based on the game rules
 * and the state of the `currBoard`, it calculates the state of the next generation and writes it to the `nextBoard`.
 */
void GameOfLife::calcNextBoard()
{
	// starting the worker right in here!
	// 'this' allows the worker thread to access all members in the background
	worker = std::async(std::launch::async, [this]()
	{
		uint16_t i = 0;

		for (uint8_t r = 0; r < rows; ++r)
		{
			for (uint8_t c = 0; c < cols; ++c)
			{
				int livingNeighbors = 0;
				for (short dr = -1; dr <= 1; ++dr)
				{
					for (short dc = -1; dc <= 1; ++dc)
					{
						if (dr == 0 && dc == 0) continue;

						int nr = r + dr;
						if (nr < 0)      nr = rows - 1;		// if we fall out the top, we come back in at the bottom.
						if (nr >= rows)  nr = 0;			// if we fall out the bottom, we come back in at the top

						int nc = c + dc;
						if (nc < 0)      nc = cols - 1;		// if we fall out on the left, we come back in on the right
						if (nc >= cols)  nc = 0;			// if we fall out on the right, we come back in on the left

						if (currBoard[nr * cols + nc] == 1)
							livingNeighbors++;
					}
				}

				// --- THE RULES CAN BE CHANGED HERE ---
				uint8_t currentState = currBoard[i];

				if (currentState == 1 && (livingNeighbors == 2 || livingNeighbors == 3))
					nextBoard[i] = 1; 	// stays alive
				else if (currentState == 0 && livingNeighbors == 3)
					nextBoard[i] = 1;	// is born
				else
					nextBoard[i] = 0;	// dies or remains dead

				++i;
			}
		}
	});
}

/**
 * @brief Checks whether a control key was pressed and performs the corresponding action
 */
bool GameOfLife::processInput()
{
	KeyInput::Key key = KeyInput::getPressedKey();

	if (key != KeyInput::Key::None)
	{
		uint16_t step = (updateTime > 345) ? 10 : 5;

		if (key == KeyInput::Key::Esc)				// ending the game
			isRunning = false;

		else if (key == KeyInput::Key::Space)		// pausing the game
		{
			isPaused = !isPaused;
			accuTime = 0;
		}

		else if (key == KeyInput::Key::Up)			// speeding up the framerate
		{
			if (updateTime > step)
				updateTime -= step;
			else
				updateTime = 1;
		}

		else if (key == KeyInput::Key::Down)		// slowing down the framerate
		{
			if (updateTime == 1) updateTime = 0;
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
