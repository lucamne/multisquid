#ifndef ENGINE_H
#define ENGINE_H
/*
 * engine.h
 *
 * Engine interface. Provides uci functions
 * plus some extras.
 *
 * TODO Make Move and Position private
 *
 * Author: Luca
 * Created: 2026/11/05
 */

#include <stdint.h>

#define NUM_PIECE_TYPES 12
#define NUM_COLORS 2

typedef uint64_t U64, Bitboard, Bitfield;
typedef struct {
	Bitboard piece_boards[NUM_PIECE_TYPES];	
	// piece bitboards
	// square mapping reference is provided in square_mapping.xlsx

	/* Incrementally Updated on Make/Unmake Move */
	unsigned int side_to_move;
	int fullmoves;
	Bitboard color_boards[NUM_COLORS];
	Bitboard occupied_board;		// all occupied squares (white | black)
	
	/* Set and Copied on Make/Unmake Move */
	Bitfield state_field;
	// bitfield of auxilary position information
	//
	// bits		| data
	// ---------------------------------------------
	// 0		= is en passant inactive
	// 1-6		= en passant square
	// 7		= White kingside castle
	// 8		= White queenside castle
	// 9		= Black kingside castle
	// 10		= Black queenside castle
	// 11-18	= halfmove clock

} Position;

typedef U64 Move;
// bitfield of move information
//
// bits		| data
// ---------------------------------------------
// 0-5		= source square
// 6-11		= destination square
// 12-15	= source piece type
// 16		= is a capture
// 17-20	= captured piece type
// 21		= is a promotion
// 22-25	= promotion piece type
// 26-29 	= castle type
// 30		= need safety check
// 31-49	= position state field

/*======================================================================
 * Functions
 *======================================================================*/
void init_engine(void);
// initializes engine
// MUST be called before using any other engine functions
int gen_pseudo_moves(Move* move_list, const Position* position);
// generate pseudo legal moves from position and store in move_list
// move_list memory should be handled by calller and be adequately large
// return number of moves in list
int load_position_from_fen(const char* fen, Position* position);
// populate supplied Position from fen
// return 0 on success; 1 on ill-formatted fen
int make_move(Position* position, Move move);
// Apply move to position
// Return 0 on move applied, 1 on move is not legal (king in check)
void print_moves(Move* move_list, int move_count);
// crude move list print out
// used for testing right now
void print_position(const Position* position);
// prints the supplied Position to stdout
long long int run_perft(const char* fen, int max_depth);
// run perft test on position given by fen to max depth
// return nodes found at max depth
void run_perft_validation(void);
// run a perft validation sweet to validate move generator

#endif
