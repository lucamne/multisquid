#include "engine.h"

#include <stdio.h>
#include <time.h>

#define STARTING_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define REPETITION "6k1/6p1/6Q1/8/8/1qq5/qqq5/5K2 w - - 0 1"
#define TEST "3r2k1/pp2p2p/5qp1/4N3/2Pp1r2/1P4QP/P1b1K3/5RR1 b - - 2 2"
#define MATE1 "1k6/ppp5/8/8/8/8/8/1K2R3 w - - 0 1"
#define MATE0 "1k2R3/ppp5/8/8/8/8/8/1K6 b - - 0 1"
#define MATE2 "1k2r3/ppp5/8/8/8/8/3R4/1K1R4 w - - 0 1"
#define STALE1 "8/8/8/5k2/7K/2R2n2/8/5q2 w - - 0 1"
#define STALE0 "8/8/8/5k2/7K/5q2/8/8 w - - 0 1"

int main(int argc, char** argv) {
	init_engine(500000000);
	// hash_test();
	// run_perft_validation();
	// run_perft(STARTING_FEN, 5);

	// play game
	/*
	Position pos = {0};
	load_position_from_fen(STARTING_FEN, &pos);
	Move move_list[256];
	char* buff[20];
	int depth = 9;

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

	// search test
	load_position_from_fen(TEST, &MULTI.pos);

	int depth = 10;
	Search_Result sr = iterative_ab_search(&MULTI.pos, depth);

	print_position(&MULTI.pos);
	printf("eval:%d\n", sr.eval);
	print_moves(sr.pv.moves, sr.pv.num_moves);

	return 0;
}
