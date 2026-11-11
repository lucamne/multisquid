#include "engine.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// used for popcount and bitscan
#ifdef WINDOWS
#include <intrin.h> 
#else
#include <stdbit.h>
#endif

/*======================================================================
 * Internal Types and Constants
 *======================================================================*/
#define ASSERT(x) assert((x))
#define NUM_SQUARES 64
#define NUM_FILES 8
#define NUM_RANKS 8
#define WHITE 0u
#define BLACK 1u

#define COLOR_OFFSET 6	// BP-WP=6,...,BK-WK=6
typedef enum {WP = 0, WN, WB, WR, WQ, WK, BP, BN, BB, BR, BQ, BK} Piece_Type;


/*======================================================================
 * U64 bitwise operations
 *======================================================================*/
static U64 get_bit(U64 u, int b) {
	ASSERT(b >= 0 && b <= 63);
	return (u >> b) & (U64)1u;
}

// get least significant bit index
static int get_ls1b(U64 u) {
	ASSERT(u);
	// TODO stdc_first_trailing_zero_ull(b); // if using stdbit.
	unsigned long index;
#ifdef WINDOWS
	_BitScanForward64(&index, u);
#endif
	return index;
}

// get most significant bit index
static int get_ms1b(U64 u) {
	ASSERT(u);
	// TODO stdc_first_trailing_zero_ull(b); // if using stdbit.
	unsigned long index;
#ifdef WINDOWS
	_BitScanReverse64(&index, u);
#endif
	return index;
}

static int get_pop_count(U64 u) {
#ifdef WINDOWS
	return (int)__popcnt64(u); 
#endif
	// TODO return (int)stdc_count_ones(*b); // if using stdbit.h
}

// populate list with indices of all '1' bits in u
static int serialize(U64 u, unsigned int* list) {
	// TODO stdc_first_trailing_zero_ull(b); // if using stdbit.
	unsigned long index;
	int count = 0;
#ifdef WINDOWS
	while(_BitScanForward64(&index, u)) {
#endif
		*list++ = index;
		ASSERT((int)index >= 0 && (int)index <= 63);
		u &= u - 1;
		count++;
	}
	return count;
}

static void set_bit(U64* u, int b) {
	ASSERT(u);
	ASSERT(b >= 0 && b <= 63);
	*u |= (U64)1u << b;
}

// set field in u of width w starting at bit b to value 
static void set_field(U64* u, U64 value, int w, int b) {
	ASSERT(u);
	ASSERT(w > 0 && w < 64);
	ASSERT(b >= 0 && b <= 63);
	ASSERT(b + w <= 64);
	ASSERT(value >> w == 0u);
	// zero field in U64
	*u &= ~((((U64)1u << w) - 1) << b);
	// set field
	*u |= value << b;
}


/*======================================================================
 * Position
 *
 * load position and provide accessors/modifiers
 *======================================================================*/
// Position state field constants
#define POS_EP_ACTIVE_SHIFT 0
#define POS_EP_SQUARE_SHIFT 1
#define POS_WK_CASTLE_SHIFT 7
#define POS_WQ_CASTLE_SHIFT 8
#define POS_BK_CASTLE_SHIFT 9
#define POS_BQ_CASTLE_SHIFT 10
#define POS_HALFMOVES_SHIFT 11
static const U64 POS_EP_ACTIVE = 0x1u << POS_EP_ACTIVE_SHIFT;
static const U64 POS_EP_SQUARE = 0x3fu << POS_EP_SQUARE_SHIFT;
static const U64 POS_WK_CASTLE = 0x1u << POS_WK_CASTLE_SHIFT;
static const U64 POS_WQ_CASTLE = 0x1u << POS_WQ_CASTLE_SHIFT;
static const U64 POS_BK_CASTLE = 0x1u << POS_BK_CASTLE_SHIFT;
static const U64 POS_BQ_CASTLE = 0x1u << POS_BQ_CASTLE_SHIFT;
static const U64 POS_HALFMOVES = 0xffu << POS_HALFMOVES_SHIFT;

static const int EMPTY_SQUARE = -1;
static const U64 RANK_MASK = 0xff;
static const int square_to_rank[NUM_SQUARES] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7};
static const int square_to_file[NUM_SQUARES] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	0, 1, 2, 3, 4, 5, 6, 7,
	0, 1, 2, 3, 4, 5, 6, 7,
	0, 1, 2, 3, 4, 5, 6, 7,
	0, 1, 2, 3, 4, 5, 6, 7,
	0, 1, 2, 3, 4, 5, 6, 7,
	0, 1, 2, 3, 4, 5, 6, 7,
	0, 1, 2, 3, 4, 5, 6, 7};

// used in ASSERT checks
static int is_valid_square(int square) { return square >= 0 && square <= 63; }
static int is_valid_pt(Piece_Type pt) { return pt >= WP && pt <= BK; }

// convert rank and file to square
static int rf_to_sq(int rank, int file) { 
	ASSERT(rank >= 0 && rank < NUM_RANKS);
	ASSERT(file >= 0 && rank < NUM_FILES);
	return rank * NUM_FILES + file;
}

static Piece_Type get_piece_at_square(const Position* p, int square) {
	ASSERT(p);
	ASSERT(is_valid_square(square));

	for (Piece_Type pt = WP; pt <= BK; pt++) {
		if (get_bit(p->piece_boards[pt], square))
			return pt;
	}

	return EMPTY_SQUARE;
}

// return a Bitboard masked by rank
static Bitboard get_rank(Bitboard b, int rank) {
	ASSERT(rank >= 0 && rank < NUM_RANKS);
	Bitboard mask = RANK_MASK << (rank * NUM_FILES);
	return b & mask;
}

static void set_piece(Position* p, Piece_Type pt, int square) {
	ASSERT(square >= 0 && square <= 63);
	ASSERT(pt >= WP && pt <= BK);
	set_bit(p->piece_boards + pt, square);
}

/* state field functions */
static U64 get_ep_active(const Position* p) {
	ASSERT(p);
	return (p->state_field & POS_EP_ACTIVE) >> POS_EP_ACTIVE_SHIFT;
}

static U64 get_ep_square(const Position* p) {
	ASSERT(p);
	return (p->state_field & POS_EP_SQUARE) >> POS_EP_SQUARE_SHIFT;
}

static U64 get_wk_castle(const Position* p) {
	ASSERT(p);
	return (p->state_field & POS_WK_CASTLE) >> POS_WK_CASTLE_SHIFT;
}

static U64 get_wq_castle(const Position* p) {
	ASSERT(p);
	return (p->state_field & POS_WQ_CASTLE) >> POS_WQ_CASTLE_SHIFT;
}

static U64 get_bk_castle(const Position* p) {
	ASSERT(p);
	return (p->state_field & POS_BK_CASTLE) >> POS_BK_CASTLE_SHIFT;
}

static U64 get_bq_castle(const Position* p) {
	ASSERT(p);
	return (p->state_field & POS_BQ_CASTLE) >> POS_BQ_CASTLE_SHIFT;
}

static U64 get_halfmoves(const Position* p) {
	ASSERT(p);
	return (p->state_field & POS_HALFMOVES) >> POS_HALFMOVES_SHIFT;
}

static void clear_ep_active(Position* p) {
	ASSERT(p);
	p->state_field &= ~POS_EP_ACTIVE;
}

static void set_ep_active(Position* p) {
	ASSERT(p);
	p->state_field |= POS_EP_ACTIVE;
}

static void set_ep_square(Position* p, int square) {
	ASSERT(p);
	ASSERT(is_valid_square(square));
	Bitfield b = p->state_field;
	p->state_field = (b & ~POS_EP_SQUARE) | 
		((U64)square << POS_EP_SQUARE_SHIFT);
}

static void clear_wk_castle(Position* p) {
	ASSERT(p);
	p->state_field &= ~POS_WK_CASTLE;
}

static void clear_wq_castle(Position* p) {
	ASSERT(p);
	p->state_field &= ~POS_WQ_CASTLE;
}

static void clear_bk_castle(Position* p) {
	ASSERT(p);
	p->state_field &= ~POS_BK_CASTLE;
}

static void clear_bq_castle(Position* p) {
	ASSERT(p);
	p->state_field &= ~POS_BQ_CASTLE;
}

static void set_wk_castle(Position* p) {
	ASSERT(p);
	p->state_field |= POS_WK_CASTLE;
}

static void set_wq_castle(Position* p) {
	ASSERT(p);
	p->state_field |= POS_WQ_CASTLE;
}

static void set_bk_castle(Position* p) {
	ASSERT(p);
	p->state_field |= POS_BK_CASTLE;
}

static void set_bq_castle(Position* p) {
	ASSERT(p);
	p->state_field |= POS_BQ_CASTLE;
}

static void set_halfmoves(Position* p, int halfmoves) {
	ASSERT(p);
	ASSERT(halfmoves <= 0xff);
	Bitfield b = p->state_field;
	p->state_field = (b & ~POS_HALFMOVES) | 
		((U64)halfmoves << POS_HALFMOVES_SHIFT);
}

// get kingside castle bit of current side to move
static U64 get_kingside_castle(const Position* p) {
	ASSERT(p);
	int castle_bit_offset = NUM_COLORS * p->side_to_move;
	return p->state_field & (POS_WK_CASTLE << castle_bit_offset);
}

// get queenside castle bit of current side to move
static U64 get_queenside_castle(const Position* p) {
	ASSERT(p);
	int castle_bit_offset = NUM_COLORS * p->side_to_move;
	return p->state_field & (POS_WQ_CASTLE << castle_bit_offset);
}

static int sq_to_rank(int square) { 
	ASSERT(is_valid_square(square));
	return square_to_rank[square];
}

static int sq_to_file(int square) { 
	ASSERT(is_valid_square(square));
	return square_to_file[square];
}

int load_position_from_fen(const char* fen, Position* position) {
	ASSERT(fen);
	ASSERT(position);

	*position = (Position){0};

	int string_pos = -1;
	int rank = 7;
	int file = 0;
	// loop through fen
	while (1) {
		char c = fen[++string_pos];

		// rank complete
		if (file >= 8) {
			if (rank > 0 && c != '/') return 1;
			if (rank == 0) break;

			file = 0;
			rank--;
			c = fen[++string_pos];
		}

		// select piece type
		Piece_Type pt;
		switch (c) {
			case 'r': pt = BR; break;
			case 'n': pt = BN; break;
			case 'b': pt = BB; break;
			case 'q': pt = BQ; break;
			case 'k': pt = BK; break;
			case 'p': pt = BP; break;
			case 'P': pt = WP; break;
			case 'R': pt = WR; break;
			case 'N': pt = WN; break;
			case 'B': pt = WB; break;
			case 'Q': pt = WQ; break;
			case 'K': pt = WK; break;
			default:
				if (c < '1' || c > '8') return 1;
				// handle empty squares
				file += c - '0';
				continue;
		}

		set_piece(position, pt, rf_to_sq(rank, file));
		file++;
	}

	// set side to move
	char c = fen[++string_pos];
	switch (c) {
		case 'w': position->side_to_move = WHITE; break;
		case 'b': position->side_to_move = BLACK; break;
		default:  return 1;
	}

	// set castle rights, expect to be zeroed initially
	string_pos++;
	c = fen[++string_pos];
	while (c != ' ') {
		switch (c) {
			case 'K': set_wk_castle(position); break;
			case 'Q': set_wq_castle(position); break;
			case 'k': set_bk_castle(position); break;
			case 'q': set_bq_castle(position); break;
			case '-': break;
			default: return 1;
		}
		c = fen[++string_pos];
	}

	// set ep square
	c = fen[++string_pos];
	if (c >= 'a' && c <= 'h') {
		file = c - 'a';
		c = fen[++string_pos];
		// error if rank is not 1-8
		if (c < '1' || c > '8') return 1;
		rank = c - '1';
		set_ep_active(position);
		set_ep_square(position, rf_to_sq(rank, file));
	} else if (c == '-') {
		clear_ep_active(position);
	} else {
		return 1;
	}

	// get halfmove clock
	string_pos+=2;
	int num_len = 0;
	c = fen[string_pos];
	if (c < '0' || c > '9') return 1;
	// find length of halfmove ascii number
	while (c >= '0' && c <= '9') c = fen[string_pos + ++num_len];

	int halfmove_sum = 0;
	for (;num_len > 0; num_len--) {
		int mult = 1;
		for(int i = 0; i < num_len - 1; i++) mult *= 10;
		halfmove_sum += mult * (fen[string_pos++] - '0');
	}
	set_halfmoves(position, halfmove_sum);

	// get fullmove clock
	if (c != ' ') return 1;
	string_pos++;

	num_len = 0;
	c = fen[string_pos];
	if (c < '0' || c > '9') return 1;
	// find length of halfmove ascii number
	while (c >= '0' && c <= '9') c = fen[string_pos + ++num_len];

	int fullmove_sum = 0;
	for (;num_len > 0; num_len--) {
		int mult = 1;
		for(int i = 0; i < num_len - 1; i++) mult *= 10;
		fullmove_sum += mult * (fen[string_pos++] - '0');
	}
	position->fullmoves = fullmove_sum;

	// init color_boards
	Bitboard white_pieces = 0u;
	Bitboard black_pieces = 0u;
	Piece_Type pt;
	for (pt = WP; pt < BP; pt++) {
		white_pieces |= position->piece_boards[pt];
	}
	for (pt = BP; pt <= BK; pt++) {
		black_pieces |= position->piece_boards[pt];
	}

	// init extra bitboards
	position->color_boards[WHITE] = white_pieces;
	position->color_boards[BLACK] = black_pieces;
	position->occupied_board = white_pieces | black_pieces;

	return 0;
}


/*======================================================================
 * Targets
 *
 * provide precomputed bitboard targets of each piece at each square
 *======================================================================*/
// use to compute sliding piece rays
typedef enum { POS_RAY = 0, NEG_RAY } Ray_Dir;
typedef struct {
	Bitboard ray_mask;			// all possible ray targets
	Bitboard quiet_targets[NUM_SQUARES];	// quiet targetes indexed by first blocker
	Bitboard capture_target[NUM_SQUARES];	// possible capture target indexed by first blocker
	Ray_Dir dir;				// ray direction (+/-)
} Ray_Targets;

// target arrays
static Bitboard KNIGHT_TARGETS[NUM_SQUARES];
static Bitboard KING_TARGETS[NUM_SQUARES];
static Bitboard PAWN_QUIET_TARGETS[NUM_COLORS][NUM_SQUARES];
static Bitboard PAWN_CAPTURE_TARGETS[NUM_COLORS][NUM_SQUARES];
static Ray_Targets RAY_UP[NUM_SQUARES];
static Ray_Targets RAY_DOWN[NUM_SQUARES];
static Ray_Targets RAY_LEFT[NUM_SQUARES];
static Ray_Targets RAY_RIGHT[NUM_SQUARES];
static Ray_Targets RAY_UP_LEFT[NUM_SQUARES];
static Ray_Targets RAY_UP_RIGHT[NUM_SQUARES];
static Ray_Targets RAY_DOWN_LEFT[NUM_SQUARES];
static Ray_Targets RAY_DOWN_RIGHT[NUM_SQUARES];

static Bitboard get_king_targets(int src) {
	ASSERT(is_valid_square(src));
	return KING_TARGETS[src];
}

static Bitboard get_knight_targets(int src) {
	ASSERT(is_valid_square(src));
	return KNIGHT_TARGETS[src];
}

static Bitboard get_pawn_capture_targets(int color, int src) {
	ASSERT(is_valid_square(src));
	ASSERT(color == WHITE || color == BLACK);
	return PAWN_CAPTURE_TARGETS[color][src];
}

static Bitboard get_pawn_quiet_targets(int color, int src) {
	ASSERT(is_valid_square(src));
	ASSERT(color == WHITE || color == BLACK);
	return PAWN_QUIET_TARGETS[color][src];
}

static const Ray_Targets* get_ray_up(int src) {
	ASSERT(is_valid_square(src));
	return RAY_UP + src;
}

static const Ray_Targets* get_ray_down(int src) {
	ASSERT(is_valid_square(src));
	return RAY_DOWN + src;
}

static const Ray_Targets* get_ray_left(int src) {
	ASSERT(is_valid_square(src));
	return RAY_LEFT + src;
}

static const Ray_Targets* get_ray_right(int src) {
	ASSERT(is_valid_square(src));
	return RAY_RIGHT + src;
}

static const Ray_Targets* get_ray_up_left(int src) {
	ASSERT(is_valid_square(src));
	return RAY_UP_LEFT + src;
}

static const Ray_Targets* get_ray_up_right(int src) {
	ASSERT(is_valid_square(src));
	return RAY_UP_RIGHT + src;
}

static const Ray_Targets* get_ray_down_left(int src) {
	ASSERT(is_valid_square(src));
	return RAY_DOWN_LEFT + src;
}

static const Ray_Targets* get_ray_down_right(int src) {
	ASSERT(is_valid_square(src));
	return RAY_DOWN_RIGHT + src;
}

static void init_king_targets(void) {
	// left shifts
	const int ur = 9;
	const int u = 8;
	const int ul = 7;
	const int r = 1;
	// right shifts
	const int l = 1;
	const int dr = 7;
	const int d = 8;
	const int dl = 9;

	for (int s = 0; s < NUM_SQUARES; s++) {
		int file = sq_to_file(s);
		int rank = sq_to_rank(s);

		Bitboard t = 0u;
		const Bitboard b = (Bitboard)1u << s;

		if (rank <= 6 && file <= 6) 	t |= b << ur;
		if (rank <= 6) 			t |= b << u;
		if (rank <= 6 && file >= 1) 	t |= b << ul;
		if (file <= 6) 			t |= b << r;

		if (file >= 1) 			t |= b >> l;
		if (rank >= 1 && file <= 6) 	t |= b >> dr;
		if (rank >= 1) 			t |= b >> d;
		if (rank >= 1 && file >= 1) 	t |= b >> dl;

		KING_TARGETS[s] = t;
	}
}

static void init_knight_targets(void) {
	// left shifts
	const int u2_r1 = 17;
	const int u2_l1 = 15;
	const int u1_r2 = 10;
	const int u1_l2 = 6;
	// right shifts
	const int d1_r2 = 6;
	const int d1_l2 = 10;
	const int d2_r1 = 15;
	const int d2_l1 = 17;

	for (int s = 0; s < NUM_SQUARES; s++) {
		int file = sq_to_file(s);
		int rank = sq_to_rank(s);

		Bitboard t = 0u;
		const Bitboard b = (Bitboard)1u << s;

		if (rank <= 5 && file <= 6) t |= b << u2_r1;
		if (rank <= 5 && file >= 1) t |= b << u2_l1;
		if (rank <= 6 && file <= 5) t |= b << u1_r2;
		if (rank <= 6 && file >= 2) t |= b << u1_l2;

		if (rank >= 1 && file <= 5) t |= b >> d1_r2;
		if (rank >= 1 && file >= 2) t |= b >> d1_l2;
		if (rank >= 2 && file <= 6) t |= b >> d2_r1;
		if (rank >= 2 && file >= 1) t |= b >> d2_l1;

		KNIGHT_TARGETS[s] = t;
	}
}

static void init_pawn_targets(void) {
	// left shifts
	const int u2 = 16;
	const int ur = 9;
	const int u = 8;
	const int ul = 7;
	// right shifts
	const int d2 = 16;
	const int dl = 9;
	const int d = 8;
	const int dr = 7;

	for (int s = 0; s < NUM_SQUARES; s++) {
		int file = sq_to_file(s);
		int rank = sq_to_rank(s);

		Bitboard w_cap_t = 0u;
		Bitboard w_quiet_t = 0u;
		Bitboard b_cap_t = 0u;
		Bitboard b_quiet_t = 0u;
		const Bitboard b = (Bitboard)1u << s;

		// captures are populated for 1st and 8th ranks because
		// they are used for checking king safety, otherwise
		// there should never be a pawn on the 1st or 8th rank

		// white pawns
		if (rank == 1) 				 w_quiet_t |= b << u2;
		if (rank >= 1 && rank <= 6) 		 w_quiet_t |= b << u;
		if (rank >= 0 && rank <= 6 && file <= 6) w_cap_t |= b << ur;
		if (rank >= 0 && rank <= 6 && file >= 1) w_cap_t |= b << ul;

		PAWN_CAPTURE_TARGETS[WHITE][s] = w_cap_t;
		PAWN_QUIET_TARGETS[WHITE][s] = w_quiet_t;

		// black pawns
		if (rank == 6)				 b_quiet_t |= b >> d2;
		if (rank <= 6 && rank >= 1)		 b_quiet_t |= b >> d;
		if (rank <= 7 && rank >= 1 && file <= 6) b_cap_t |= b >> dr;
		if (rank <= 7 && rank >= 1 && file >= 1) b_cap_t |= b >> dl;

		PAWN_CAPTURE_TARGETS[BLACK][s] = b_cap_t;
		PAWN_QUIET_TARGETS[BLACK][s] = b_quiet_t;
	}
}

static void init_sliding_targets(void) {
	// left shifts
	const int u = 8;
	const int r = 1;
	const int ul = 7;
	const int ur = 9;
	// right shifts
	const int d = 8;
	const int l = 1;
	const int dr = 7;
	const int dl = 9;

	for (int s = 0; s < NUM_SQUARES; s++) {
		Bitboard ray;
		const Bitboard b = (Bitboard)1u << s;
		const Bitboard one_bit = (Bitboard)1u;

		int rank = sq_to_rank(s);
		int file = sq_to_file(s);
		int max_up = NUM_RANKS - rank;
		int max_down = rank - 0;
		int max_right = NUM_FILES - file;
		int max_left = file - 0;

		// UP
		// create ray_mask
		ray = (Bitboard)0u;
		for (int i = 1; i < max_up; i++) {
			ray |= b << (i * u);
		}
		RAY_UP[s].ray_mask = ray;
		RAY_UP[s].dir = POS_RAY;

		// compute move targets
		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (!i) targets = 0u;
			else set_field(&targets, 0, NUM_SQUARES - i, i);

			RAY_UP[s].quiet_targets[i] = targets;
			RAY_UP[s].capture_target[i] = ray & (one_bit << i);
		}

		// RIGHT
		ray = (Bitboard)0u;
		for (int i = 1; i < max_right; i++) {
			ray |= b << (i * r);
		}
		RAY_RIGHT[s].ray_mask = ray;
		RAY_RIGHT[s].dir = POS_RAY;

		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (!i) targets = 0u;
			else set_field(&targets, 0, NUM_SQUARES - i, i);

			RAY_RIGHT[s].quiet_targets[i] = targets;
			RAY_RIGHT[s].capture_target[i] = ray & (one_bit << i);
		}

		// DOWN
		ray = (Bitboard)0u;
		for (int i = 1; i <= max_down; i++) {
			ray |= b >> (i * d);
		}
		RAY_DOWN[s].ray_mask = ray;
		RAY_DOWN[s].dir = NEG_RAY;

		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (i == NUM_SQUARES - 1) targets = 0u;
			else set_field(&targets, 0, i + 1, 0);

			RAY_DOWN[s].quiet_targets[i] = targets;
			RAY_DOWN[s].capture_target[i] = ray & (one_bit << i);
		}

		// LEFT
		ray = (Bitboard)0u;
		for (int i = 1; i <= max_left; i++) {
			ray |= b >> (i * l);
		}
		RAY_LEFT[s].ray_mask = ray;
		RAY_LEFT[s].dir = NEG_RAY;

		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (i == NUM_SQUARES - 1) targets = 0u;
			else set_field(&targets, 0, i + 1, 0);

			RAY_LEFT[s].quiet_targets[i] = targets;
			RAY_LEFT[s].capture_target[i] = ray & (one_bit << i);
		}

		// UP-LEFT
		ray = (Bitboard)0u;
		for (int i = 1; i < max_up && i <= max_left; i++) {
			ray |= b << (i * ul);
		}
		RAY_UP_LEFT[s].ray_mask = ray;
		RAY_UP_LEFT[s].dir = POS_RAY;

		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (!i) targets = 0u;
			else set_field(&targets, 0, NUM_SQUARES - i, i);

			RAY_UP_LEFT[s].quiet_targets[i] = targets;
			RAY_UP_LEFT[s].capture_target[i] = ray & (one_bit << i);
		}


		// UP-RIGHT
		ray = (Bitboard)0u;
		for (int i = 1; i < max_up && i < max_right; i++) {
			ray |= b << (i * ur);
		}
		RAY_UP_RIGHT[s].ray_mask = ray;
		RAY_UP_RIGHT[s].dir = POS_RAY;

		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (!i) targets = 0u;
			else set_field(&targets, 0, NUM_SQUARES - i, i);

			RAY_UP_RIGHT[s].quiet_targets[i] = targets;
			RAY_UP_RIGHT[s].capture_target[i] = ray & (one_bit << i);
		}

		// DOWN-RIGHT
		ray = (Bitboard)0u;
		for (int i = 1; i <= max_down && i < max_right; i++) {
			ray |= b >> (i * dr);
		}
		RAY_DOWN_RIGHT[s].ray_mask = ray;
		RAY_DOWN_RIGHT[s].dir = NEG_RAY;

		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (i == NUM_SQUARES - 1) targets = 0u;
			else set_field(&targets, 0, i + 1, 0);

			RAY_DOWN_RIGHT[s].quiet_targets[i] = targets;
			RAY_DOWN_RIGHT[s].capture_target[i] = ray & (one_bit << i);
		}

		// DOWN-LEFT
		ray = (Bitboard)0u;
		for (int i = 1; i <= max_down && i <= max_left; i++) {
			ray |= b >> (i * dl);
		}
		RAY_DOWN_LEFT[s].ray_mask = ray;
		RAY_DOWN_LEFT[s].dir = NEG_RAY;

		for (int i = 0; i < NUM_SQUARES; i++) {
			Bitboard targets = ray;
			if (i == NUM_SQUARES - 1) targets = 0u;
			else set_field(&targets, 0, i + 1, 0);

			RAY_DOWN_LEFT[s].quiet_targets[i] = targets;
			RAY_DOWN_LEFT[s].capture_target[i] = ray & (one_bit << i);
		}
	}
}

// call once during engine initialization
static void init_targets(void) {
	init_knight_targets();
	init_king_targets();
	init_pawn_targets();
	init_sliding_targets();
}


/*======================================================================
 * Move
 *
 * provides move generation and move making functions
 *======================================================================*/
#define MOVE_LIST_SIZE 256
#define NUM_SLIDING_TYPES_PER_SIDE 3

// castle field constants
#define NO_CASTLE_TYPE 0u
#define WK_CASTLE_TYPE 0b1u
#define WQ_CASTLE_TYPE 0b10u
#define BK_CASTLE_TYPE 0b100u
#define BQ_CASTLE_TYPE 0b1000u

// Move bitfield constants
#define MOVE_SRC_SHIFT 0
#define MOVE_DEST_SHIFT 6
#define MOVE_SRC_PT_SHIFT 12
#define MOVE_IS_CAP_SHIFT 16
#define MOVE_CAP_PT_SHIFT 17
#define MOVE_IS_PROMO_SHIFT 21
#define MOVE_PROMO_PT_SHIFT 22
#define MOVE_CASTLE_TYPE_SHIFT 26
#define MOVE_NEED_SAFETY_CHECK_SHIFT 30
#define MOVE_STATE_COPY_SHIFT 31
static const U64 MOVE_SRC = 0x3fu << MOVE_SRC_SHIFT;
static const U64 MOVE_DEST = 0x3fu << MOVE_DEST_SHIFT;
static const U64 MOVE_SRC_PT = 0xfu << MOVE_SRC_PT_SHIFT;
static const U64 MOVE_IS_CAP = 0x1u << MOVE_IS_CAP_SHIFT;
static const U64 MOVE_CAP_PT = 0xfu << MOVE_CAP_PT_SHIFT;
static const U64 MOVE_IS_PROMO = 0x1u << MOVE_IS_PROMO_SHIFT;
static const U64 MOVE_PROMO_PT = 0xfu << MOVE_PROMO_PT_SHIFT;
static const U64 MOVE_CASTLE_TYPE = 0xfu << MOVE_CASTLE_TYPE_SHIFT;
static const U64 MOVE_NEED_SAFETY_CHECK = 0x1u << MOVE_NEED_SAFETY_CHECK_SHIFT;
static const U64 MOVE_STATE_COPY = 0x7ffffu << MOVE_STATE_COPY_SHIFT;

// passed to pack_move to generate a Move bitfield
typedef struct {
	int src;
	int dest;
	Piece_Type src_pt;
	int is_cap;
	Piece_Type cap_pt;
	int is_promo;
	Piece_Type promo_pt;
	unsigned int castle_type;
	int need_safety_check;		// see if king is in check after move
					// Bitfield state_field;	// preserves position state field
} Unpacked_Move;

// used in ASSERT calls
static int is_valid_castle_type(unsigned int castle_type) {
	return castle_type == NO_CASTLE_TYPE || 
		castle_type == WK_CASTLE_TYPE || 
		castle_type == WQ_CASTLE_TYPE || 
		castle_type == BK_CASTLE_TYPE || 
		castle_type == BQ_CASTLE_TYPE;
}

/* packed move getters */
static U64 get_move_src(Move m) {
	return (m & MOVE_SRC) >> MOVE_SRC_SHIFT;
}

static U64 get_move_dest(Move m) {
	return (m & MOVE_DEST) >> MOVE_DEST_SHIFT;
}

static U64 get_move_src_pt(Move m) {
	return (m & MOVE_SRC_PT) >> MOVE_SRC_PT_SHIFT;
}

static U64 get_move_promo_pt(Move m) {
	return (m & MOVE_PROMO_PT) >> MOVE_PROMO_PT_SHIFT;
}

static U64 get_move_is_promo(Move m) {
	return !!(m & MOVE_IS_PROMO);
}

static U64 get_move_is_capture(Move m) {
	return !!(m & MOVE_IS_CAP);
}

static U64 get_move_cap_pt(Move m) {
	return (m & MOVE_CAP_PT) >> MOVE_CAP_PT_SHIFT;
}

static U64 get_move_need_safety_check(Move m) {
	return (m & MOVE_NEED_SAFETY_CHECK) >> MOVE_NEED_SAFETY_CHECK_SHIFT;
}

static U64 get_move_castle_type(Move m) {
	return (m & MOVE_CASTLE_TYPE) >> MOVE_CASTLE_TYPE_SHIFT;
}

/* pack move setters */
static void set_move_src(Move* m, int square) {
	ASSERT(m);
	ASSERT(is_valid_square(square));
	*m = (*m & ~MOVE_SRC) | ((U64)square << MOVE_SRC_SHIFT);
}

static void set_move_dest(Move* m, int square) {
	ASSERT(m);
	ASSERT(is_valid_square(square));
	*m = (*m & ~MOVE_DEST) | ((U64)square << MOVE_DEST_SHIFT);
}

static void set_move_src_pt(Move* m, Piece_Type pt) {
	ASSERT(m);
	ASSERT(is_valid_pt(pt));
	*m = (*m & ~MOVE_SRC_PT) | ((U64)pt << MOVE_SRC_PT_SHIFT);
}

static void set_move_is_capture_value(Move* m, int v) {
	ASSERT(m);
	ASSERT(v == 0 || v == 1);
	*m |= (U64)v << MOVE_IS_CAP_SHIFT;
}

static void set_move_cap_pt(Move* m, Piece_Type pt) {
	ASSERT(m);
	ASSERT(is_valid_pt(pt));
	*m = (*m & ~MOVE_CAP_PT) | ((U64)pt << MOVE_CAP_PT_SHIFT);
}

static void set_move_is_promo_value(Move* m, int v) {
	ASSERT(m);
	ASSERT(v == 0 || v == 1);
	*m |= (U64)v << MOVE_IS_PROMO_SHIFT;
}

static void set_move_promo_pt(Move* m, Piece_Type pt) {
	ASSERT(m);
	ASSERT(is_valid_pt(pt));
	*m = (*m & ~MOVE_PROMO_PT) | ((U64)pt << MOVE_PROMO_PT_SHIFT);
}

static void set_move_castle_type(Move* m, unsigned int castle_type) {
	ASSERT(m);
	ASSERT(is_valid_castle_type(castle_type));
	*m = (*m & ~MOVE_CASTLE_TYPE) | 
		((U64)castle_type << MOVE_CASTLE_TYPE_SHIFT);
}

static void set_move_need_safety_check(Move* m, int nsc) {
	ASSERT(m);
	ASSERT(nsc == 1 || nsc == 0);
	*m |= (U64)nsc << MOVE_NEED_SAFETY_CHECK_SHIFT;
}

/*
   static void set_move_state_copy(Move* m, U64 state) {
   ASSERT(m);
   ASSERT(!(state >> MOVE_STATE_COPY_SHIFT));
 *m = (*m & ~MOVE_STATE_COPY) | ((U64)state << MOVE_STATE_COPY_SHIFT);
 }
 */

// generate a packed Move bitfield from Unpacked_Move
static Move pack_move(const Unpacked_Move* m) {
	ASSERT(is_valid_square(m->src));
	ASSERT(is_valid_square(m->dest));
	ASSERT(is_valid_pt(m->src_pt));
	ASSERT(m->is_cap == 0 || m->is_cap == 1);
	ASSERT(is_valid_pt(m->cap_pt));
	ASSERT(m->cap_pt != WK || m->cap_pt != BK);
	ASSERT(m->is_promo == 0 || m->is_promo == 1);
	ASSERT(is_valid_pt(m->promo_pt));
	ASSERT(
			!m->is_promo || (
				m->promo_pt != WP &&
				m->promo_pt != BP &&
				m->promo_pt != WK &&
				m->promo_pt != BK));
	ASSERT(is_valid_castle_type(m->castle_type));
	ASSERT(m->need_safety_check == 1 || m->need_safety_check == 0);

	Move move = (Move)0u;
	set_move_src(&move, m->src);
	set_move_dest(&move, m->dest);
	set_move_src_pt(&move, m->src_pt);
	set_move_is_capture_value(&move, m->is_cap);
	set_move_cap_pt(&move, m->cap_pt);
	set_move_is_promo_value(&move, m->is_promo);
	set_move_promo_pt(&move, m->promo_pt);
	set_move_castle_type(&move, m->castle_type);
	set_move_need_safety_check(&move, m->need_safety_check);
	// set_move_state_copy(&move, m->state_field);
	return move;
}

// add a move from a 'targets' bitboard to move list
// moves will be added to index 0 of move_list
// move is assumed to be a standard capture 
// 	(not promo, not en_passant, not castle)
static int add_basic_capture(
		const Position* position,
		Move* move_list,
		Piece_Type pt,
		int src,
		Bitboard targets,
		int need_safety_check) {

	ASSERT(position);
	ASSERT(move_list);
	ASSERT(is_valid_pt(pt));
	ASSERT(is_valid_square(src));

	if (!targets) return 0;

	// serialiaze target bitboard into target array
	int moves_added = 0;
	unsigned int target_squares[NUM_SQUARES];
	int target_count = serialize(
			targets, 
			target_squares);

	// iterate through target squares, add move to move_list
	for (int j = 0; j < target_count; j++) {
		int dest = target_squares[j];
		Piece_Type cap_pt = get_piece_at_square(position, dest);
		ASSERT(is_valid_pt(cap_pt));

		Unpacked_Move unpacked_move = {
			src,
			dest,
			pt,
			1,
			cap_pt,
			0,
			0,
			0u,
			need_safety_check
				//,position->state_field
		};
		move_list[moves_added++] = pack_move(&unpacked_move);
	}
	return moves_added;
}

// add a move from a 'targets' bitboard to a move list
// moves will be added to index 0 of move_list
// move is assumed to be a quiet move 
// 	(not promo, not castle)
static int add_basic_quiet(
		const Position* position,
		Move* move_list,
		Piece_Type pt,
		int src,
		Bitboard targets,
		int need_safety_check) {

	ASSERT(position);
	ASSERT(move_list);
	ASSERT(is_valid_pt(pt));
	ASSERT(is_valid_square(src));
	ASSERT(!(pt == WP && sq_to_rank(src) == 6) && 
			!(pt == BP && sq_to_rank(src) == 1));

	if (!targets) return 0;

	// serialiaze target bitboard into target array
	int moves_added = 0;
	unsigned int target_squares[NUM_SQUARES];
	int target_count = serialize(
			targets, 
			target_squares);

	// iterate through target squares, add move to move_list
	for (int j = 0; j < target_count; j++) {
		int dest = target_squares[j];

		Unpacked_Move unpacked_move = {
			src,
			dest,
			pt,
			0,
			0,
			0,
			0,
			0u,
			need_safety_check
				//,position->state_field
		};
		move_list[moves_added++] = pack_move(&unpacked_move);
	}
	return moves_added;
}

// add castle moves if possible
// does not check for king moving through check (handled by make_move)
static int add_castle_moves(
		const Position* position, 
		Move* move_list) {

	ASSERT(position);
	ASSERT(move_list);

	int moves_added = 0;
	int side_to_move = position->side_to_move;
	Bitboard occupied = position->occupied_board;
	Piece_Type pt = WK + COLOR_OFFSET * side_to_move;

	Bitboard king_castle_mask = 
		(Bitboard)0b1100000 << (7 * NUM_FILES * side_to_move);
	Bitboard queen_castle_mask = 
		(Bitboard)0b1110 << (7 * NUM_FILES * side_to_move);

	Bitboard king_castle = get_kingside_castle(position);
	Bitboard queen_castle = get_queenside_castle(position);

	// add kingside castle if side has the right and intermediate squares
	// are empty
	if (king_castle && !(occupied & king_castle_mask)) {
		ASSERT(
				(!side_to_move && 
				 get_piece_at_square(position, 4) == WK) || 
				(side_to_move && 
				 get_piece_at_square(position, 60) == BK));
		ASSERT(
				(!side_to_move && 
				 get_piece_at_square(position, 7) == WR) || 
				(side_to_move && 
				 get_piece_at_square(position, 63) == BR));

		Unpacked_Move unpacked_move = {
			4 + NUM_FILES * (NUM_RANKS - 1) * side_to_move,
			6 + NUM_FILES * (NUM_RANKS - 1) * side_to_move,
			pt,
			0,
			0,
			0,
			0,
			WK_CASTLE_TYPE << (NUM_COLORS * side_to_move),
			1
				//,position->state_field
		};
		move_list[moves_added++] = pack_move(&unpacked_move);
	}
	// add queenside castle if side has the right and intermediate squares
	// are empty
	if (queen_castle && !(occupied & queen_castle_mask)) {
		ASSERT(
				(!side_to_move && 
				 get_piece_at_square(position, 4) == WK) || 
				(side_to_move && 
				 get_piece_at_square(position, 60) == BK));
		ASSERT(
				(!side_to_move && 
				 get_piece_at_square(position, 0) == WR) || 
				(side_to_move && 
				 get_piece_at_square(position, 56) == BR));

		Unpacked_Move unpacked_move = {
			4 + NUM_FILES * (NUM_RANKS - 1) * side_to_move,
			2 + NUM_FILES * (NUM_RANKS - 1) * side_to_move,
			pt,
			0,
			0,
			0,
			0,
			WQ_CASTLE_TYPE << (NUM_COLORS * side_to_move),
			1
				//,position->state_field
		};
		move_list[moves_added++] = pack_move(&unpacked_move);
	}
	return moves_added;
}

// add a move from a 'targets' bitboard to a move list
// moves will be added to index 0 of move_list
// move is assumed to be an en passant capture
static int add_ep_capture(
		const Position* position,
		Move* move_list,
		Piece_Type pt,
		int src,
		Bitboard targets,
		int need_safety_check) {

	ASSERT(position);
	ASSERT(move_list);
	ASSERT(pt == WP || pt == BP);
	ASSERT(is_valid_square(src));

	if (!targets) return 0;

	int side_to_move = position->side_to_move;
	int dest = get_ls1b(targets);
	Piece_Type cap_pt = get_piece_at_square(
			position, 
			dest - NUM_FILES + (2 * NUM_FILES * side_to_move));

	ASSERT(dest == get_ep_square(position));
	ASSERT(cap_pt == WP || cap_pt == BP);

	Unpacked_Move unpacked_move = {
		src,
		dest,
		pt,
		1,
		cap_pt,
		0,
		0,
		0u,
		need_safety_check
			//,position->state_field
	};
	*move_list = pack_move(&unpacked_move);
	return 1;
}

// add a move from a 'targets' bitboard to a move list
// moves will be added to index 0 of move_list
// move is assumed to be a pawn promotion capture 
static int add_promo_capture(
		const Position* position,
		Move* move_list,
		Piece_Type pt,
		int src,
		Bitboard targets,
		int need_safety_check) {

	ASSERT(position);
	ASSERT(move_list);
	ASSERT(pt == WP || pt == BP);
	ASSERT(is_valid_square(src));
	ASSERT((pt == WP && sq_to_rank(src) == 6) || 
			(pt == BP && sq_to_rank(src) == 1));

	if (!targets) return 0;

	// serialiaze target bitboard into target array
	int moves_added = 0;
	unsigned int target_squares[NUM_SQUARES];
	int target_count = serialize(
			targets, 
			target_squares);

	// iterate through target squares, add move to move_list
	for (int j = 0; j < target_count; j++) {
		int dest = target_squares[j];
		Piece_Type cap_pt = get_piece_at_square(position, dest);
		ASSERT(is_valid_pt(cap_pt));

		// insert move for each promo type
		int color_offset = COLOR_OFFSET * position->side_to_move;
		Unpacked_Move unpacked_move = {
			src,
			dest,
			pt,
			1,
			cap_pt,
			1,
			WQ + color_offset,
			0u,
			need_safety_check
				//,position->state_field
		};
		move_list[moves_added++] = pack_move(&unpacked_move);

		unpacked_move.promo_pt = WR + color_offset;
		move_list[moves_added++] = pack_move(&unpacked_move);

		unpacked_move.promo_pt = WB + color_offset;
		move_list[moves_added++] = pack_move(&unpacked_move);

		unpacked_move.promo_pt = WN + color_offset;
		move_list[moves_added++] = pack_move(&unpacked_move);
	}
	return moves_added;
}

// add a move from a 'targets' bitboard to a move list
// moves will be added to index 0 of move_list
// move is assumed to be a pawn promotion quiet move (not a capture) 
static int add_promo_quiet(
		const Position* position,
		Move* move_list,
		Piece_Type pt,
		int src,
		Bitboard targets,
		int need_safety_check) {

	ASSERT(position);
	ASSERT(move_list);
	ASSERT(pt == WP || pt == BP);
	ASSERT(is_valid_square(src));
	ASSERT((pt == WP && sq_to_rank(src) == 6) || 
			(pt == BP && sq_to_rank(src) == 1));

	if (!targets) return 0;

	// serialiaze target bitboard into target array
	int moves_added = 0;
	unsigned int target_squares[NUM_SQUARES];
	int target_count = serialize(
			targets, 
			target_squares);

	// iterate through target squares, add move to move_list
	for (int j = 0; j < target_count; j++) {
		int dest = target_squares[j];

		// insert move for each promo type
		int color_offset = COLOR_OFFSET * position->side_to_move;
		Unpacked_Move unpacked_move = {
			src,
			dest,
			pt,
			0,
			0,
			1,
			WQ + color_offset,
			0u,
			need_safety_check
				//,position->state_field
		};
		move_list[moves_added++] = pack_move(&unpacked_move);

		unpacked_move.promo_pt = WR + color_offset;
		move_list[moves_added++] = pack_move(&unpacked_move);

		unpacked_move.promo_pt = WB + color_offset;
		move_list[moves_added++] = pack_move(&unpacked_move);

		unpacked_move.promo_pt = WN + color_offset;
		move_list[moves_added++] = pack_move(&unpacked_move);
	}
	return moves_added;
}

// generate king moves and add to either tactical or quiet move list
// update num_ values
static void gen_king_moves(
		const Position* position, 
		Move* tact_moves, 
		Move* quiet_moves, 
		int* num_tact_moves, 
		int* num_quiet_moves,
		Bitboard opp_disjoint_targets) {

	ASSERT(position);
	ASSERT(tact_moves);
	ASSERT(quiet_moves);
	ASSERT(num_tact_moves);
	ASSERT(num_quiet_moves);

	int side_to_move = (int)position->side_to_move;
	Piece_Type pt = WK + side_to_move * COLOR_OFFSET;
	Bitboard same_color_pieces = position->color_boards[side_to_move];
	Bitboard opp_color_pieces = position->color_boards[side_to_move ^ 1u];

	int src = get_ls1b(position->piece_boards[pt]);
	ASSERT(src);

	// get legal targets
	Bitboard targets = get_king_targets(src);
	targets &= ~same_color_pieces;
	// remove squares where king is definitely in check
	targets &= ~opp_disjoint_targets;
	Bitboard capture_targets = targets & opp_color_pieces;
	Bitboard quiet_targets = targets ^ capture_targets;

	*num_tact_moves += add_basic_capture(
			position, 
			tact_moves + *num_tact_moves,
			pt, 
			src,
			capture_targets,
			1);

	*num_quiet_moves += add_basic_quiet(
			position, 
			quiet_moves + *num_quiet_moves,
			pt, 
			src,
			quiet_targets,
			1);

	if (!get_bit(opp_disjoint_targets, src)) {
		*num_quiet_moves += add_castle_moves(
				position, quiet_moves + *num_quiet_moves);
	}
}

// generate knight moves and add to either tactical or quiet move list
// update num_ values
static void gen_knight_moves(
		const Position* position, 
		Move* tact_moves, 
		Move* quiet_moves, 
		int* num_tact_moves, 
		int* num_quiet_moves,
		int king_in_check,
		int king_xrayed) {

	ASSERT(position);
	ASSERT(tact_moves);
	ASSERT(quiet_moves);
	ASSERT(num_tact_moves);
	ASSERT(num_quiet_moves);

	int side_to_move = (int)position->side_to_move;
	Piece_Type pt = WN + side_to_move * COLOR_OFFSET;
	Bitboard knight_board = position->piece_boards[pt];

	if (!knight_board) return;

	int need_safety_check = king_in_check || king_xrayed;
	Bitboard same_color_pieces = position->color_boards[side_to_move];
	Bitboard opp_color_pieces = position->color_boards[side_to_move ^ 1u];

	// serialize targets into array
	unsigned int piece_squares[NUM_SQUARES];
	int piece_count = serialize(knight_board, piece_squares);

	// iterate through 'src' squares
	for (int i = 0; i < piece_count; i++) {
		int src = piece_squares[i];

		// get legal targets
		Bitboard targets = get_knight_targets(src);
		targets &= ~same_color_pieces;
		Bitboard capture_targets = targets & opp_color_pieces;

		*num_tact_moves += add_basic_capture(
				position, 
				tact_moves + *num_tact_moves,
				pt, 
				src,
				capture_targets,
				need_safety_check);

		if (!king_in_check) {
			Bitboard quiet_targets = targets ^ capture_targets;
			*num_quiet_moves += add_basic_quiet(
					position, 
					quiet_moves + *num_quiet_moves,
					pt, 
					src,
					quiet_targets,
					need_safety_check);
		}
	}
}

// generate pawn promos and add to tactical move list
// promos may be capturs or not
// update num_tact_moves
static void gen_pawn_promos(
		const Position* position, 
		Move* tact_moves, 
		int* num_tact_moves,
		int king_in_check,
		int king_xrayed) {

	ASSERT(position);
	ASSERT(tact_moves);
	ASSERT(num_tact_moves);

	// get pawn by color
	int side_to_move = (int)position->side_to_move;
	Piece_Type pt = WP + side_to_move * COLOR_OFFSET;

	// get pawns on pre-promo rank (WHITE=6, BLACK=1)
	Bitboard pawn_board = position->piece_boards[pt];
	pawn_board = get_rank(pawn_board, 6 - 5 * side_to_move);

	if (!pawn_board) return;

	int need_safety_check = king_in_check || king_xrayed;
	Bitboard opp_color_pieces = position->color_boards[side_to_move ^ 1u];
	Bitboard occupied = position->occupied_board;

	// serialize targets into array
	unsigned int piece_squares[NUM_SQUARES];
	int piece_count = serialize(pawn_board, piece_squares);

	// iterate through 'src' squares
	for (int i = 0; i < piece_count; i++) {
		int src = piece_squares[i];

		// capture_targets
		Bitboard capture_targets = 
			get_pawn_capture_targets(side_to_move, src);
		capture_targets &= opp_color_pieces;

		*num_tact_moves += add_promo_capture(
				position, 
				tact_moves + *num_tact_moves,
				pt, 
				src,
				capture_targets,
				need_safety_check);

		if (!king_in_check) {
			// quiet_targets
			Bitboard quiet_targets = 
				get_pawn_quiet_targets(side_to_move, src);
			quiet_targets &= ~occupied;

			*num_tact_moves += add_promo_quiet(
					position, 
					tact_moves + *num_tact_moves,
					pt, 
					src,
					quiet_targets,
					need_safety_check);
		}
	}
}

// generate moves for pawns not promoting and not on starting squares
// add moves to tactical move list or quiet move list
// update num_ values
static void gen_pawn_standard_moves(
		const Position* position, 
		Move* tact_moves, 
		Move* quiet_moves, 
		int* num_tact_moves, 
		int* num_quiet_moves,
		int king_in_check,
		int king_xrayed) {

	ASSERT(position);
	ASSERT(tact_moves);
	ASSERT(quiet_moves);
	ASSERT(num_tact_moves);
	ASSERT(num_quiet_moves);


	// get pawn by color
	int side_to_move = (int)position->side_to_move;
	Piece_Type pt = WP + side_to_move * COLOR_OFFSET;

	// extract ranks 3 through 6
	// ranks 2 and 7 are handled in special pawn generators
	// in a legal position pawns cannot be on 1 and 8
	const Bitboard STANDARD_PAWN_MOVE_MASK = (Bitboard)0xffffffff << 16;
	Bitboard pawn_board = position->piece_boards[pt] & STANDARD_PAWN_MOVE_MASK;

	if (!pawn_board) return;

	Bitboard occupied = position->occupied_board;
	Bitboard opp_color_pieces = position->color_boards[side_to_move ^ 1u];
	int need_safety_check = king_in_check || king_xrayed;

	// get en passant bit board
	Bitboard ep_board = (Bitboard)1u * get_ep_active(position);
	ep_board = ep_board << get_ep_square(position);

	// serialize targets into array
	unsigned int piece_squares[NUM_SQUARES];
	int piece_count = serialize(pawn_board, piece_squares);

	// iterate through 'src' squares
	for (int i = 0; i < piece_count; i++) {
		int src = piece_squares[i];

		// capture_targets
		Bitboard capture_targets = 
			get_pawn_capture_targets(side_to_move, src);
		Bitboard ep_capture_target = capture_targets & ep_board;
		capture_targets &= opp_color_pieces;


		*num_tact_moves += add_basic_capture(
				position, 
				tact_moves + *num_tact_moves,
				pt, 
				src,
				capture_targets,
				need_safety_check);

		*num_tact_moves += add_ep_capture(
				position, 
				tact_moves + *num_tact_moves,
				pt, 
				src,
				ep_capture_target,
				need_safety_check);

		if (!king_in_check) {
			// quiet_targets
			Bitboard quiet_targets = 
				get_pawn_quiet_targets(side_to_move, src);
			quiet_targets &= ~occupied;

			*num_quiet_moves += add_basic_quiet(
					position, 
					quiet_moves + *num_quiet_moves,
					pt, 
					src,
					quiet_targets,
					need_safety_check);
		}
	}
}

// generate moves for pawns on starting squares and add to either tactical or quiet move list
// update num_ values
static void gen_pawn_start_moves(
		const Position* position, 
		Move* tact_moves, 
		Move* quiet_moves, 
		int* num_tact_moves, 
		int* num_quiet_moves,
		int king_in_check,
		int king_xrayed) {

	ASSERT(position);
	ASSERT(tact_moves);
	ASSERT(quiet_moves);
	ASSERT(num_tact_moves);
	ASSERT(num_quiet_moves);

	// get pawn by color
	int side_to_move = (int)position->side_to_move;
	Piece_Type pt = WP + side_to_move * COLOR_OFFSET;

	// pawns on start squars are isolated using start rank
	// pieces in front of starting pawns are copied to copy
	// 	rank in order to block double pawn move
	int start_rank = 1 + 5 * side_to_move;
	int blocking_rank = 2 + 3 * side_to_move;
	int copy_rank = 3 + 1 * side_to_move;

	// get pawns on start rank (WHITE=1, BLACK=6)
	Bitboard pawn_board = position->piece_boards[pt];
	pawn_board = get_rank(pawn_board, start_rank);

	if (!pawn_board) return;

	int need_safety_check = king_in_check || king_xrayed;
	Bitboard opp_color_pieces = position->color_boards[side_to_move ^ 1u];
	Bitboard occupied = position->occupied_board;

	Bitboard blocking_rank_board = get_rank(occupied, blocking_rank); 
	blocking_rank_board = blocking_rank_board >> (blocking_rank * NUM_FILES);
	blocking_rank_board = blocking_rank_board << (copy_rank * NUM_FILES);
	occupied |= blocking_rank_board;

	// serialize targets into array
	unsigned int piece_squares[NUM_SQUARES];
	int piece_count = serialize(pawn_board, piece_squares);

	// iterate through 'src' squares
	for (int i = 0; i < piece_count; i++) {
		int src = piece_squares[i];

		// capture_targets
		Bitboard capture_targets = 
			get_pawn_capture_targets(side_to_move, src);
		capture_targets &= opp_color_pieces;

		*num_tact_moves += add_basic_capture(
				position, 
				tact_moves + *num_tact_moves,
				pt, 
				src,
				capture_targets,
				need_safety_check);

		if (!king_in_check) {
			// get quiet_targets
			// use modified bitboard to prevent double pawn push 
			// when first square is blocked
			Bitboard quiet_targets = 
				get_pawn_quiet_targets(side_to_move, src);
			quiet_targets &= ~occupied;

			*num_quiet_moves += add_basic_quiet(
					position, 
					quiet_moves + *num_quiet_moves,
					pt, 
					src,
					quiet_targets,
					need_safety_check);
		}
	}
}

// generate pawn moves and add to either tactical or quiet move list
// update num_ values
static void gen_pawn_moves(
		const Position* position, 
		Move* tact_moves, 
		Move* quiet_moves, 
		int* num_tact_moves, 
		int* num_quiet_moves,
		int king_in_check,
		int king_xrayed) {

	ASSERT(position);
	ASSERT(tact_moves);
	ASSERT(quiet_moves);
	ASSERT(num_tact_moves);
	ASSERT(num_quiet_moves);

	gen_pawn_promos(
			position, 
			tact_moves, 
			num_tact_moves, 
			king_in_check,
			king_xrayed);
	gen_pawn_start_moves(
			position, 
			tact_moves, 
			quiet_moves, 
			num_tact_moves, 
			num_quiet_moves,
			king_in_check,
			king_xrayed);
	gen_pawn_standard_moves(
			position, 
			tact_moves, 
			quiet_moves, 
			num_tact_moves, 
			num_quiet_moves,
			king_in_check,
			king_xrayed);
}

// add by union to capture and quiet target bitboards from given Ray_Targets
static void union_targets_from_ray(
		const Position* position, 
		const Ray_Targets* ray_targets, 
		Bitboard* capture_targets,
		Bitboard* quiet_targets) {

	ASSERT(position);
	ASSERT(ray_targets);
	ASSERT(capture_targets);
	ASSERT(quiet_targets);

	Bitboard opp_color_pieces = 
		position->color_boards[position->side_to_move ^ 1u];
	Bitboard occupied = position->occupied_board;

	// retrieve ray mask, check if any squares in ray are occupied
	Bitboard ray_mask = ray_targets->ray_mask;
	Bitboard ray_squares = ray_mask & occupied;

	// if any ray squares are occupied find blockers
	if (ray_squares) {
		int blocker_index;
		// find first or last occupied square depending on ray direction
		if (ray_targets->dir == POS_RAY)
			blocker_index = get_ls1b(ray_squares);
		else
			blocker_index = get_ms1b(ray_squares);

		// potential capture target 
		// interesect with opposite colored pieces
		*capture_targets |= ray_targets->capture_target[blocker_index] & 
			opp_color_pieces;

		// quiet targets
		*quiet_targets |= ray_targets->quiet_targets[blocker_index];
	} else {
		// if no blocker, quiet targets are full ray
		*quiet_targets |= ray_mask;
	}
}

// generate queen moves and add to either tactical or quiet move list
// update num_ values
static void gen_sliding_moves(
		const Position* position, 
		Move* tact_moves, 
		Move* quiet_moves, 
		int* num_tact_moves, 
		int* num_quiet_moves,
		int king_in_check,
		int king_xrayed) {

	ASSERT(position);
	ASSERT(tact_moves);
	ASSERT(quiet_moves);
	ASSERT(num_tact_moves);
	ASSERT(num_quiet_moves);
	ASSERT(king_in_check == 0 || king_in_check == 1);
	ASSERT(king_xrayed == 0 || king_xrayed == 1);

	int side_to_move = (int)position->side_to_move;
	int need_safety_check = king_in_check || king_xrayed;

	// iterate through sliding piece types
	Piece_Type sliders[NUM_SLIDING_TYPES_PER_SIDE] = {WB, WR, WQ};
	for (
			int sliders_i = 0; 
			sliders_i < NUM_SLIDING_TYPES_PER_SIDE; 
			sliders_i++) {

		Piece_Type pt = sliders[sliders_i] + side_to_move * COLOR_OFFSET;
		Bitboard piece_board = position->piece_boards[pt];

		if (!piece_board) continue;

		// serialize targets into array
		unsigned int piece_squares[NUM_SQUARES];
		int piece_count = 
			serialize(piece_board, piece_squares);

		// iterate through 'src' squares
		for (int i = 0; i < piece_count; i++) {
			int src = piece_squares[i];
			Bitboard capture_targets = 0u;
			Bitboard quiet_targets = 0u;

			// generate diagonal slides for bishops and queens
			if (sliders_i == 0 || sliders_i == 2) {
				const Ray_Targets* up_right = 
					get_ray_up_right(src);
				const Ray_Targets* up_left = 
					get_ray_up_left(src);
				const Ray_Targets* down_right = 
					get_ray_down_right(src);
				const Ray_Targets* down_left = 
					get_ray_down_left(src);

				union_targets_from_ray(position, up_right, &capture_targets, &quiet_targets);
				union_targets_from_ray(position, up_left, &capture_targets, &quiet_targets);
				union_targets_from_ray(position, down_right, &capture_targets, &quiet_targets);
				union_targets_from_ray(position, down_left, &capture_targets, &quiet_targets);
			}

			// generate straight line slides for rooks and queens
			if (sliders_i == 1 || sliders_i == 2) {
				const Ray_Targets* up = get_ray_up(src);
				const Ray_Targets* down = get_ray_down(src);
				const Ray_Targets* left = get_ray_left(src);
				const Ray_Targets* right = get_ray_right(src);

				union_targets_from_ray(position, up, &capture_targets, &quiet_targets);
				union_targets_from_ray(position, down, &capture_targets, &quiet_targets);
				union_targets_from_ray(position, left, &capture_targets, &quiet_targets);
				union_targets_from_ray(position, right, &capture_targets, &quiet_targets);
			}

			if (capture_targets) {
				*num_tact_moves += add_basic_capture(
						position, 
						tact_moves + *num_tact_moves, 
						pt, 
						src, 
						capture_targets,
						need_safety_check);
			}
			if (quiet_targets && !king_in_check) {
				*num_quiet_moves += add_basic_quiet(
						position, 
						quiet_moves + *num_quiet_moves, 
						pt, 
						src, 
						quiet_targets,
						need_safety_check);
			}
		}
	}
}

// get all disjoint capture targets from side (kings/knights/pawns)
static Bitboard get_disjoint_targets(const Position* position, int side) {
	Piece_Type pt;
	unsigned int target_squares[NUM_SQUARES];
	int target_count;
	Bitboard disjoint_targets = 0u;
	const Bitboard* piece_boards = position->piece_boards;
	Bitboard piece_board;

	// pawns
	pt = WP + COLOR_OFFSET * side;
	piece_board = piece_boards[pt];
	if (piece_board) {
		target_count = serialize(piece_board, target_squares);
		for (int i = 0; i < target_count; i++) {
			disjoint_targets |= 
				get_pawn_capture_targets(side, target_squares[i]);
		}
	}

	// knights
	pt = WN + COLOR_OFFSET * side;
	piece_board = piece_boards[pt];
	if (piece_board) {
		target_count = serialize(piece_board, target_squares);
		for (int i = 0; i < target_count; i++) {
			disjoint_targets |= get_knight_targets(target_squares[i]);
		}
	}

	// king
	pt = WK + COLOR_OFFSET * side;
	int king_square = get_ls1b(position->piece_boards[pt]);
	disjoint_targets |= get_king_targets(king_square);

	return disjoint_targets;
}

// return a bitboard of all sliding piece moves as if the board is empty
static Bitboard get_xray_targets(const Position* position, int side) {
	Piece_Type pt0, pt1;
	const Bitboard* piece_boards = position->piece_boards;
	Bitboard combined_pieces;
	Bitboard xrays = 0u;
	unsigned int target_squares[NUM_SQUARES];
	int target_count;

	// straight-line pieces
	pt0 = WR + COLOR_OFFSET * side;
	pt1 = WQ + COLOR_OFFSET * side;
	combined_pieces = piece_boards[pt0] | piece_boards[pt1];

	if (combined_pieces) {
		target_count = serialize(combined_pieces, target_squares);

		for (int i = 0; i < target_count; i++) {
			int sq = target_squares[i];
			xrays |= get_ray_up(sq)->ray_mask;
			xrays |= get_ray_down(sq)->ray_mask;
			xrays |= get_ray_left(sq)->ray_mask;
			xrays |= get_ray_right(sq)->ray_mask;
		}
	}

	// diagonal pieces
	pt0 = WB + COLOR_OFFSET * side;
	pt1 = WQ + COLOR_OFFSET * side;
	combined_pieces = piece_boards[pt0] | piece_boards[pt1];

	if (combined_pieces) {
		target_count = serialize(combined_pieces, target_squares);

		for (int i = 0; i < target_count; i++) {
			int sq = target_squares[i];
			xrays |= get_ray_up_left(sq)->ray_mask;
			xrays |= get_ray_up_right(sq)->ray_mask;
			xrays |= get_ray_down_left(sq)->ray_mask;
			xrays |= get_ray_down_right(sq)->ray_mask;
		}
	}

	return xrays;
}

int gen_pseudo_moves(Move* move_list, const Position* position) {
	ASSERT(move_list);
	ASSERT(position);

	int side_to_move = (int)position->side_to_move;
	int opp_side = side_to_move ^ 1u;

	// use to chunk moves into capture and quiet moves
	// tactical moves include promotion moves
	Move tact_moves[MOVE_LIST_SIZE];
	Move quiet_moves[MOVE_LIST_SIZE];
	int num_tact_moves = 0;
	int num_quiet_moves = 0;

	Bitboard king_square = 
		position->piece_boards[WK + COLOR_OFFSET * side_to_move];

	// find out if king is in check from disjoint targets
	// if so, only captures and king moves are allowed
	Bitboard opp_disjoint_targets = 
		get_disjoint_targets(position, opp_side);
	int king_in_check = !!(king_square & opp_disjoint_targets);

	// find out if king is x-rayed, if king is x-rayed or in check
	// a king safety check will need to be run on make move
	Bitboard opp_xrays = get_xray_targets(position, opp_side);
	int king_xrayed = !!(opp_xrays & king_square);

	gen_pawn_moves(
			position, 
			tact_moves, 
			quiet_moves, 
			&num_tact_moves, 
			&num_quiet_moves,
			king_in_check,
			king_xrayed);
	gen_knight_moves(
			position, 
			tact_moves, 
			quiet_moves, 
			&num_tact_moves, 
			&num_quiet_moves,
			king_in_check,
			king_xrayed);
	gen_sliding_moves(
			position, 
			tact_moves, 
			quiet_moves, 
			&num_tact_moves, 
			&num_quiet_moves,
			king_in_check,
			king_xrayed);
	gen_king_moves(
			position, 
			tact_moves, 
			quiet_moves, 
			&num_tact_moves, 
			&num_quiet_moves,
			opp_disjoint_targets);

	// copy capture and quiet moves into move list
	int total_moves = num_tact_moves + num_quiet_moves;
	ASSERT(total_moves <= MOVE_LIST_SIZE);

	memcpy(
			move_list, 
			tact_moves, 
			num_tact_moves * sizeof(Move));
	memcpy(
			move_list + num_tact_moves, 
			quiet_moves, 
			num_quiet_moves * sizeof(Move));

	return total_moves;
}

// swap source piece from src to dest, handle castling as well
// could be used to make or unmake move
static void make_move_update_bitboards(Position* position, Move move) {
	const int side_to_move = (int)position->side_to_move;
	const int src = (int)get_move_src(move);
	const int dest = (int)get_move_dest(move);
	const Piece_Type src_pt = get_move_src_pt(move);
	const unsigned int castle_type = (unsigned int)get_move_castle_type(move);
	Bitboard* const piece_boards = position->piece_boards;

	// dest piece is the same as source piece unless move is a promo
	Piece_Type dest_pt = src_pt; 
	if (get_move_is_promo(move)) {
		dest_pt = get_move_promo_pt(move);
		ASSERT(is_valid_pt(dest_pt));
		ASSERT(
				dest_pt != WK || 
				dest_pt != BK || 
				dest_pt != WP || 
				dest_pt != BP);
	}

	ASSERT(is_valid_square(src));
	ASSERT(is_valid_square(dest));
	ASSERT(is_valid_pt(src_pt));
	ASSERT(is_valid_pt(dest_pt));
	ASSERT(is_valid_castle_type(castle_type));

	// flip source piece
	Bitboard src_swapper = (Bitboard)1u << src;
	piece_boards[src_pt] ^= src_swapper;
	position->color_boards[side_to_move] ^= src_swapper;

	// flip dest piece 
	Bitboard dest_swapper = (Bitboard)1u << dest;
	piece_boards[dest_pt] ^= dest_swapper;
	position->color_boards[side_to_move] ^= dest_swapper;

	// update rook bitboards for a castle move
	// king is handled through src and dest fields
	const Bitboard krook_swap = (Bitboard)0b10100000u;
	const Bitboard qrook_swap = (Bitboard)0b1001;
	const int black_castle_shift = NUM_FILES * (NUM_RANKS - 1);
	if (castle_type) {
		ASSERT(src_pt == WK + COLOR_OFFSET * side_to_move);

		Bitboard rook_swapper = 0u;
		switch (castle_type) {
			case WK_CASTLE_TYPE:
				ASSERT(!side_to_move);
				rook_swapper = krook_swap;
				break;

			case WQ_CASTLE_TYPE:
				ASSERT(!side_to_move);
				rook_swapper = qrook_swap;
				break;

			case BK_CASTLE_TYPE:
				ASSERT(side_to_move);
				rook_swapper = krook_swap << black_castle_shift;
				break;

			case BQ_CASTLE_TYPE:
				ASSERT(side_to_move);
				rook_swapper = qrook_swap << black_castle_shift;
				break;
			default:
				ASSERT(0);
		}
		// flip rooks
		Piece_Type rook_pt = WR + COLOR_OFFSET * side_to_move;
		piece_boards[rook_pt] ^= rook_swapper;
		position->color_boards[side_to_move] ^= rook_swapper;

	} else if (get_move_is_capture(move)) {
		int cap_square = dest;
		Piece_Type cap_pt = get_move_cap_pt(move);
		ASSERT(is_valid_pt(cap_pt));

		// update capture piece if move is an ep capture
		if (get_ep_active(position)) {
			int ep_square = (int)get_ep_square(position);
			if (dest == ep_square) {
				ASSERT(src_pt == (int)(WP + COLOR_OFFSET * 
							side_to_move));
				ASSERT(cap_pt == (int)(WP + COLOR_OFFSET * 
							(side_to_move ^ 1u)));

				cap_square += -NUM_FILES + 
					(2 * NUM_FILES * side_to_move);
			}
		}
		// flip capture piece
		Bitboard cap_swapper = (Bitboard)1u << cap_square;
		piece_boards[cap_pt] ^= cap_swapper;
		position->color_boards[side_to_move ^ 1u] ^= cap_swapper;
	}
	position->occupied_board = position->color_boards[WHITE] | 
		position->color_boards[BLACK];
}

// return 1 if square is attacked by side, 0 if not
static int is_square_attacked(const Position* position, int square, int side) {
	ASSERT(position);
	ASSERT(is_valid_square(square));
	ASSERT(side == WHITE || side == BLACK);

	Bitboard occupied = position->occupied_board;
	const Bitboard* piece_boards = position->piece_boards;
	const Ray_Targets* ray;
	Bitboard targets;
	Piece_Type pt0, pt1;

	// Pretend each piece type is at the square to test.
	// If the fake piece could capture a piece of the
	// same type, then the square is attacked because that
	// piece could attack the square to test as well.

	// disjoint
	// knights
	pt0 = WN + COLOR_OFFSET * side;
	targets = get_knight_targets(square);
	if (piece_boards[pt0] & targets) return 1;
	// king
	pt0 = WK + COLOR_OFFSET * side;
	targets = get_king_targets(square);
	if (piece_boards[pt0] & targets) return 1;
	// pawns
	pt0 = WP + COLOR_OFFSET * side;
	targets = get_pawn_capture_targets(side ^ 1u, square);
	if (piece_boards[pt0] & targets) return 1;

	// straight line sliders
	// only check if rooks or queens are on the board
	pt0 = WR + COLOR_OFFSET * side;
	pt1 = WQ + COLOR_OFFSET * side;
	Bitboard straight_line_board = piece_boards[pt0] | piece_boards[pt1];

	if (straight_line_board) {
		ray = get_ray_up(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ls1b(targets);
			if (get_bit(straight_line_board, blocker)){
				return 1;
			}
		}

		ray = get_ray_right(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ls1b(targets);
			if (get_bit(straight_line_board, blocker)) {
				return 1;
			}
		}

		ray = get_ray_left(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ms1b(targets);
			if (get_bit(straight_line_board, blocker)) {
				return 1;
			}
		}

		ray = get_ray_down(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ms1b(targets);
			if (get_bit(straight_line_board, blocker)) {
				return 1;
			}
		}
	}

	// diagonal sliders
	// only check if bishops or queens are on the board
	pt0 = WB + COLOR_OFFSET * side;
	pt1 = WQ + COLOR_OFFSET * side;
	Bitboard diagonal_board = piece_boards[pt0] | piece_boards[pt1];

	if (diagonal_board) {
		ray = get_ray_up_right(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ls1b(targets);
			if (get_bit(diagonal_board, blocker)) {
				return 1;
			}
		}

		ray = get_ray_up_left(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ls1b(targets);
			if (get_bit(diagonal_board, blocker)) {
				return 1;
			}
		}

		ray = get_ray_down_right(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ms1b(targets);
			if (get_bit(diagonal_board, blocker)) {
				return 1;
			}
		}

		ray = get_ray_down_left(square);
		targets = ray->ray_mask & occupied;
		if (targets) {
			int blocker = get_ms1b(targets);
			if (get_bit(diagonal_board, blocker)) {
				return 1;
			}
		}
	}

	return 0;
}

int make_move(Position* position, Move move) {
	ASSERT(position);

	Piece_Type src_pt = get_move_src_pt(move);
	int side_to_move = position->side_to_move;
	int src = (int)get_move_src(move);
	int dest = (int)get_move_dest(move);

	// constant for castling checks
	const int WK_SQUARE = 4;
	const int WRK_SQUARE = 7;
	const int WRQ_SQUARE = 0;
	const int BK_SQUARE = 60;
	const int BRK_SQUARE = 63;
	const int BRQ_SQUARE = 56;

	// swap src and dest on bitboards
	make_move_update_bitboards(position, move);

	// safety check if needed
	if (get_move_need_safety_check(move)) {
		int king_square = 
			get_ls1b(position->piece_boards[WK + COLOR_OFFSET * side_to_move]);
		if (is_square_attacked(position, king_square, side_to_move ^ 1u)) {
			// make_move_update_bitboards(position, move);
			return 1;
		}
	}

	// test castle move is fully legal
	U64 castle_type = get_move_castle_type(move);
	if (castle_type) {
		// can't castle out of check
		if (is_square_attacked(position, src, !side_to_move)) { 
			// make_move_update_bitboards(position, move);
			return 1;
		}

		switch (castle_type) {
			case WK_CASTLE_TYPE:
				ASSERT(!side_to_move);

				if (is_square_attacked(position, WK_SQUARE + 1, BLACK)) {
					// make_move_update_bitboards(position, move);
					return 1;
				}
				break;

			case WQ_CASTLE_TYPE:
				ASSERT(!side_to_move);

				if (is_square_attacked(position, WK_SQUARE - 1, BLACK)) {
					// make_move_update_bitboards(position, move);
					return 1;
				}
				break;

			case BK_CASTLE_TYPE:
				ASSERT(side_to_move);

				if (is_square_attacked(position, BK_SQUARE + 1, WHITE)) {
					// make_move_update_bitboards(position, move);
					return 1;
				}
				break;

			case BQ_CASTLE_TYPE:
				ASSERT(side_to_move);

				if (is_square_attacked(position, BK_SQUARE - 1, WHITE)) {
					// make_move_update_bitboards(position, move);
					return 1;
				}
				break;
			default:
				ASSERT(0);
		}
	}

	// incremental updates
	ASSERT(side_to_move == 1 || side_to_move == 0);
	position->fullmoves += side_to_move;
	position->side_to_move = side_to_move ^ 1u;

	// update ep square
	if (src_pt == WP + COLOR_OFFSET * side_to_move &&
			dest - (src - 4 * NUM_FILES * side_to_move) == 2 * NUM_FILES) {
		set_ep_active(position);
		set_ep_square(position, dest - NUM_FILES + (2 * NUM_FILES * side_to_move));
	} else {
		clear_ep_active(position);
	}

	// update castle rights
	if (src == WK_SQUARE || src == WRK_SQUARE || dest == WRK_SQUARE)
		clear_wk_castle(position);
	if (src == WK_SQUARE || src == WRQ_SQUARE || dest == WRQ_SQUARE)
		clear_wq_castle(position);
	if (src == BK_SQUARE || src == BRK_SQUARE || dest == BRK_SQUARE)
		clear_bk_castle(position);
	if (src == BK_SQUARE || src == BRQ_SQUARE || dest == BRQ_SQUARE)
		clear_bq_castle(position);

	// update halfmoves
	if (get_move_is_capture(move) || src_pt == WP + COLOR_OFFSET * side_to_move)
		set_halfmoves(position, 0);
	else
		set_halfmoves(position, (int)get_halfmoves(position) + 1);

	return 0;
}

/*
   void unmake_move(Position* position, Move move) {
// copy state
position->state_field = move >> MOVE_STATE_COPY_SHIFT;
// undo incremental update
position->side_to_move ^= 1u;
ASSERT(position->side_to_move == 0 || position->side_to_move == 1);
position->fullmoves -= position->side_to_move;
// undo bitboard
make_move_update_bitboards(position, move);
}
*/

/*======================================================================
 * Initialization
 *======================================================================*/
void init_engine(void) {
	init_targets();
}


/*======================================================================
 * Debugging
 *======================================================================*/
#define NUM_TEST_POSITIONS 6

void print_position(const Position* position) {
	printf("\n    =================================\n");
	for (int rank = 7; rank >= 0; rank--) {
		printf("  %i |", rank + 1);
		for (int file = 0; file < 8; file++) {
			int square = rf_to_sq(rank, file);	
			Piece_Type pt = get_piece_at_square(position, square);

			char c;
			switch (pt) {
				case WP: c = 'P'; break;
				case WN: c = 'N'; break;
				case WB: c = 'B'; break;
				case WR: c = 'R'; break;
				case WQ: c = 'Q'; break;
				case WK: c = 'K'; break;
				case BP: c = 'p'; break;
				case BN: c = 'n'; break;
				case BB: c = 'b'; break;
				case BR: c = 'r'; break;
				case BQ: c = 'q'; break;
				case BK: c = 'k'; break;
				default: c = ' '; break;
			}

			printf(" %c |", c);
		}
		printf("\n    =================================\n");
	}
	printf("      a   b   c   d   e   f   g   h\n");

	/* print aux information */
	// side to move
	if (position->side_to_move) printf("\n    side to move: black");
	else printf("\n    side to move: white");

	// castle rights
	printf("\n    castle rights: ");
	if (get_wk_castle(position)) printf("K");
	if (get_wq_castle(position)) printf("Q");
	if (get_bk_castle(position)) printf("k");
	if (get_bq_castle(position)) printf("q");

	printf("\n    en passant: ");
	if (!get_ep_active(position)) {
		printf("-");
	} else {
		int square = (int)get_ep_square(position);
		int file = sq_to_file(square);
		int rank = sq_to_rank(square);
		char f = 'a' + (char)file;
		char r = '1' + (char)rank;
		printf("%c%c", f, r);
	}

	// halfmoves
	int halfmoves = (int)get_halfmoves(position);
	printf("\n    halfmoves: %i", halfmoves);

	// fullmoves
	printf("\n    fullmoves: %i\n\n", position->fullmoves);
}

void print_moves(Move* move_list, int move_count) {
	for (int i = 0; i < move_count; i++) {
		Move m = move_list[i];

		int src = (int)get_move_src(m);
		int dest = (int)get_move_dest(m);

		char src_file = (char)sq_to_file(src) + 'a';
		char src_rank = (char)sq_to_rank(src) + '1';

		char dest_file = (char)sq_to_file(dest) + 'a';
		char dest_rank = (char)sq_to_rank(dest) + '1';

		printf("\n%i   %c%c->%c%c", 
				i,
				src_file, 
				src_rank, 
				dest_file, 
				dest_rank);
	}
}

static long long int perft(Position* position, int depth) {
	if (!depth) return 1;

	Move move_list[MOVE_LIST_SIZE];
	int move_count = gen_pseudo_moves(move_list, position);

	long long int node_count = 0;
	for (int i = 0; i < move_count; i++) {
		Position temp_pos = *position;
		if (make_move(&temp_pos, move_list[i]))
			continue;
		if (depth == 1)
			node_count++;
		else
			node_count += perft(&temp_pos, depth - 1);
		// unmake_move(position, move_list[i]);
	}

	return node_count;
}

long long int run_perft(const char* fen, int max_depth) {
	Position position = {0};
	load_position_from_fen(fen, &position);

	printf("\nperft\nposition=%s\nmax depth=%i\n\n", fen, max_depth);

	clock_t start = clock();

	long long int node_count = 0;
	long long int total_nodes = 0;
	for (int d = 1; d <= max_depth; d++) {
		node_count = perft(&position, d);
		total_nodes += node_count;
		printf("depth=%i; nodes=%lli\n", d, node_count);
	}
	printf("\n");

	clock_t end = clock();
	double time_elapsed = (double)(end - start) / CLOCKS_PER_SEC;
	long long int nodes_per_sec = (long long int)((double)total_nodes / time_elapsed);

	printf("total nodes=%lli\ntime elapsed=%fs\nnps=%lli\n",
			total_nodes, time_elapsed, nodes_per_sec);

	return node_count;
}

void run_perft_validation(void) {
	char* fens[NUM_TEST_POSITIONS] = {
		"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
		"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
		"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1 ",
		"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
		"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8  ",
		"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10 "};
	int depths[NUM_TEST_POSITIONS] = {6, 5, 6, 5, 5, 5};
	long long int results[NUM_TEST_POSITIONS] = {
		119060324,
		193690690,
		11030083,
		15833292,
		89941194,
		164075551
	};

	for (int i = 0; i < NUM_TEST_POSITIONS; i++) {
		long long int node_count = run_perft(fens[i], depths[i]);
		if (node_count != results[i]) {
			printf("\nPERFT FAILED!!!\n");
			printf("position=%s\n", fens[i]);
			printf("nodes found=%lli\n", node_count);
			printf("nodes expected=%lli\n\n", results[i]);
			return;
		}
	}
}
