#include "engine.h"

#include <stdio.h>

/*
// TODO TEMP STUFF FOR TESTING START
#include <stdio.h>
static const int SRC_S = 0;
static const int DEST_S = 6;
static const int SRC_W = 6;
static const int DEST_W = 6;
*/

#define MOVE_LIST_SIZE 256
// TEMP STUFF FOR TESTING END

#define STARTING_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

int main(int argc, char** argv) {
	init_engine();
	// run_perft(STARTING_FEN, 7);
	run_perft_validation();

	return 0;
}
