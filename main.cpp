#include "GameOfLife.h"


// execution example
int main()
{
	GameOfLife game(30, 30);	// 30 x 30 board
	game.setRandomness(25);		// randomness 25% (0 to 100%)
	game.start_loop(33);		// 33 ms updateTime (1 to 1000 ms)

	return 0;
}
