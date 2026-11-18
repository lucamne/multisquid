#include "engine.h"

#include <stdio.h>

#define STARTING_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define TEST "3r4/ppp4p/8/nk6/3PRRP1/2P1K3/PP5P/8 w - - 1 26"
#define MATE1 "1k6/ppp5/8/8/8/8/8/1K2R3 w - - 0 1"
#define MATE0 "1k2R3/ppp5/8/8/8/8/8/1K6 b - - 0 1"
#define MATE2 "1k2r3/ppp5/8/8/8/8/3R4/1K1R4 w - - 0 1"
#define STALE1 "8/8/8/5k2/7K/2R2n2/8/5q2 w - - 0 1"
#define STALE0 "8/8/8/5k2/7K/5q2/8/8 w - - 0 1"

int main(int argc, char** argv) {
	init_engine();
	hash_test();
	run_perft_validation();
	// run_perft(STARTING_FEN, 5);

	/*
	// play game
	Position pos = {0};
	load_position_from_fen(TEST, &pos);
	Move move_list[256];
	char* buff[20];
	int depth = 8;

	while (1) {
		print_position(&pos);
		int mc = gen_pseudo_moves(move_list, &pos);
		print_moves(move_list, mc);
		int i;
		scanf("%i", &i);

		Position copy_pos = pos;
		if (make_move(&copy_pos, move_list[i])) continue;
		pos = copy_pos;

		print_position(&pos);

		Search_Result sr = iterative_ab_search(&pos, depth);
		make_move(&pos, sr.pv.moves[0]);
		printf("eval:%d\n", sr.eval);
		print_moves(sr.pv.moves, 1);
	}
	*/



	/*
	// search test
	Position pos = {0};
	load_position_from_fen(TEST, &pos);

	int depth = 12;
	Search_Result sr = iterative_ab_search(&pos, depth);

	print_position(&pos);
	printf("eval:%d\n", sr.eval);
	print_moves(sr.pv.moves, sr.pv.num_moves);
	*/

	return 0;
}
