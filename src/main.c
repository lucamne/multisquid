#include "engine.h"

#include <stdio.h>

#define STARTING_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define TEST "8/8/8/8/8/8/Q7/1K6 w KQkq - 0 1"

int main(int argc, char** argv) {
	init_engine();
	// run_perft(STARTING_FEN, 5);
	// run_perft_validation();

	Position pos = {0};
	load_position_from_fen(STARTING_FEN, &pos);

	int depth = 8;
	Search_Result sr = iterative_ab_search(&pos, depth);

	print_position(&pos);
	printf("eval:%d\n", sr.eval);
	print_moves(sr.pv.moves, sr.pv.num_moves);

	return 0;
}
