#ifndef ENGINE_H
#define ENGINE_H
/*
 * engine.h
 *
 * Engine interface. Provides uci functions
 * plus some extras.
 *
 * TODO lots of stuff can be made private eventually
 *
 * Author: Luca
 * Created: 2026/11/05
 */

#include <stdint.h>

/* Position */
#define NUM_PIECE_TYPES 12
#define NUM_PIECE_TYPES_PER_SIDE 6
#define NUM_COLORS 2

typedef uint64_t U64, Bitboard, Bitfield, Hash;

typedef struct {
	// piece bitboards
	// square mapping reference is provided in square_mapping.xlsx
	Bitboard piece_boards[NUM_PIECE_TYPES];	

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

	// piece square values, updated incrementally during make_move()
	int psq_mg;
	int psq_eg;
	int gamephase;

	// unique(ish) hash of position
	U64 hash;
} Position;

/* Move */
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

/* Search */
#define MAX_PV_LINE 50
typedef struct {
	int num_moves;
	Move moves[MAX_PV_LINE];
} PV_Line;

typedef struct {
	int eval;
	PV_Line pv;
} Search_Result;

/* Transposition Table */
// stored in node type
typedef enum {
	EMPTY_ENTRY = 0,		// TT_Entry is not set
	PV_NODE,
	ALL_NODE, 
	CUT_NODE
} TTNode_Type;

typedef struct {
	Hash hash;			// position hash
	unsigned char node_type;	// see defines
	unsigned char depth;		// depth to search of position
	int score;			// position eval
	Move best_move;			// best move in position, 0 if none
} TT_Entry;

/*** Global Multisquid Engine Memory ***/
struct Multisquid {
	Position pos;

	// shared hash table
	TT_Entry* tt;
	int tt_size;
	Hash tt_key;
} MULTI;
// this memory contains all state information for the engine
// initialized during init_engine()
// careful when messing with it :)

/*======================================================================
 * Functions
 *======================================================================*/
Search_Result iterative_ab_search(const Position* position, int depth);
// run an iteratively deepening alpha-beta search to depth
void init_engine(U64 max_tt_bytes);
// initializes engine with a maximum transposition table size in bytes
// MUST be called before using any other engine functions
int load_position_from_fen(const char* fen, Position* position);
// populate supplied Position from fen
// return 0 on success; 1 on ill-formatted fen
void print_moves(Move* move_list, int move_count);
// crude move list print out
// used for testing right now
void print_position(const Position* position);
// prints the supplied Position to stdout
void run_perft_validation(void);
// run a perft validation sweet to validate move generator
void hash_test(void);
// test position hashes are generated correctly

#endif
