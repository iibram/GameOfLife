#include "GameOfLife.h"

// Windows-specific header (for the timer tweak)
#ifdef _WIN32
#include <windows.h>
#endif


// execution example
int main()
{
	// force Windows timer to maximum precision (1ms)
	#ifdef _WIN32
	UINT targetTimerResolution = 1;
	bool timerTargetSet = (timeBeginPeriod(targetTimerResolution) == TIMERR_NOERROR);
	#endif

	//GameOfLife game(40, 40, Figure::PENTADECATHLON);	// a 40 x 40 board with a single PENTADECATHLON starting in the center (randomness is irrelevant)
	//GameOfLife game(40, 40, 25);						// a 40 x 40 board with a randomness of 25% (0 to 100) for each cell to start as "alive"
	GameOfLife game(30, 30);							// a 30 x 30 board with a standard randomness of 20% for each cell to start as "alive"

	// reset the Windows timer
	#ifdef _WIN32
	if (timerTargetSet)
		timeEndPeriod(targetTimerResolution);
	#endif

	return 0;
}
