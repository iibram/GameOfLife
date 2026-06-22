#include "GameOfLife.h"


// execution examples
int main()
{

	//GameOfLife game(40, 40, Figure::PENTADECATHLON);	// a 40 x 40 board with a single PENTADECATHLON starting in the center (randomness is irrelevant)
	//GameOfLife game(40, 40, 25);						// a 40 x 40 board with a randomness of 25% (0 to 100) for each cell to start as "alive"
	GameOfLife game(30, 30);							// a 30 x 30 board with a standard randomness of 20% for each cell to start as "alive"

	return 0;
}
