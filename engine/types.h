#ifndef CHESS_ENGINE_TYPES_H_
#define CHESS_ENGINE_TYPES_H_

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "utils.h"

namespace engine
{

using Depth = int32_t;
constexpr Depth MAX_DEPTH = 40;

const int32_t MAX_PLIES = 800;
constexpr int MAX_MOVES = 512;
constexpr int MAX_PINS = 16;

using Bitboard = uint64_t;

enum Color : uint32_t
{
    WHITE,
    BLACK,
};
constexpr uint32_t COLOR_NUM = 2;

enum Square : uint32_t
{
    // clang-format off
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    NO_SQUARE
    // clang-format on
};
constexpr uint32_t SQUARE_NUM = 64;

enum PieceKind : uint32_t
{
    NO_PIECE_KIND,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING
};
constexpr uint32_t PIECE_KIND_NUM = 7;

enum Piece : uint32_t
{
    NO_PIECE,
    W_PAWN,
    W_KNIGHT,
    W_BISHOP,
    W_ROOK,
    W_QUEEN,
    W_KING,
    B_PAWN,
    B_KNIGHT,
    B_BISHOP,
    B_ROOK,
    B_QUEEN,
    B_KING,
};
constexpr uint32_t PIECE_NUM = 13;

enum Rank : int32_t
{
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
};
constexpr uint32_t RANK_NUM = 8;

enum File : int32_t
{
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
};
constexpr uint32_t FILE_NUM = 8;

enum Castling : uint32_t
{
    NO_CASTLING = 0,
    W_OO = 1 << 0,
    W_OOO = 1 << 1,
    B_OO = 1 << 2,
    B_OOO = 1 << 3,
    W_CASTLING = W_OO | W_OOO,
    B_CASTLING = B_OO | B_OOO,
    KING_CASTLING = W_OO | B_OO,
    QUEEN_CASTLING = W_OOO | B_OOO,
    ALL_CASTLING = W_CASTLING | B_CASTLING
};

enum GamePhase : uint32_t
{
    MIDDLE_GAME = 0,
    END_GAME = 1
};
constexpr uint32_t GAME_PHASE_NUM = 2;

// encoded move
// 0-5 - from
// 6-11 - to
// 12-14 - promotion
// 15-16 - castling
using Move = uint32_t;

Square from(Move move);
Square to(Move move);
PieceKind promotion(Move move);
Castling castling(Move move);

constexpr Move create_move(Square from, Square to)
{
    assert(from != NO_SQUARE);
    assert(to != NO_SQUARE);
    return to << 6 | from;
}

constexpr Move create_promotion(Square from, Square to, PieceKind promotion)
{
    assert(from != NO_SQUARE);
    assert(to != NO_SQUARE);
    assert(promotion != PAWN);
    assert(promotion != KING);
    return promotion << 12 | to << 6 | from;
}

constexpr Move create_castling(Castling castling)
{
    assert(castling == KING_CASTLING || castling == QUEEN_CASTLING);
    return static_cast<Move>((castling == KING_CASTLING ? 1 : 2) << 15);
}


constexpr Move NO_MOVE = 0;
constexpr Move KING_CASTLING_MOVE = create_castling(KING_CASTLING);
constexpr Move QUEEN_CASTLING_MOVE = create_castling(QUEEN_CASTLING);


// encoded move information
// 0-2 - captured piece
// 3-6 - last castling rights
// 7-12 - last enpassant square
// 13-13 - last enpassant
// 14-14 - enpassant
// 15-22 - half move counter
using MoveInfo = uint32_t;

MoveInfo create_moveinfo(PieceKind captured, Castling last_castling,
                         Square last_enpassant, bool enpassant,
                         uint8_t half_move_counter);

PieceKind captured_piece(MoveInfo moveinfo);
Castling last_castling(MoveInfo moveinfo);
Square last_enpassant_square(MoveInfo moveinfo);
bool enpassant(MoveInfo moveinfo);
uint8_t half_move_counter(MoveInfo moveinfo);

const Castling CASTLING_RIGHTS[COLOR_NUM] = {W_CASTLING, B_CASTLING};
const Square KING_SIDE_ROOK_SQUARE[COLOR_NUM] = {SQ_H1, SQ_H8};
const Square QUEEN_SIDE_ROOK_SQUARE[COLOR_NUM] = {SQ_A1, SQ_A8};

constexpr Rank rank(Square sq)
{
    ASSERT(sq != NO_SQUARE);
    return Rank(sq >> 3);
}

constexpr File file(Square sq)
{
    ASSERT(sq != NO_SQUARE);
    return File(sq & 7);
}

constexpr Color sq_color(Square sq)
{
    ASSERT(sq != NO_SQUARE);
    return (static_cast<int>(rank(sq)) + static_cast<int>(file(sq))) % 2 ? WHITE : BLACK;
}

constexpr Square make_square(Rank rank, File file)
{
    ASSERT(RANK_1 <= rank && rank <= RANK_8);
    ASSERT(FILE_A <= file && file <= FILE_H);
    return Square((rank << 3) + file);
}

constexpr Square flip_vertically(Square sq)
{
    return make_square(Rank(RANK_8 - rank(sq)), file(sq));
}

constexpr Square flip_horizontally(Square sq)
{
    return make_square(rank(sq), File(FILE_H - file(sq)));
}

constexpr Square normalize(Square sq, Color side)
{
    return side == WHITE ? sq : flip_vertically(sq);
}

#define ENABLE_BASIC_OPERATIONS(T)                                       \
    constexpr T operator+(T v1, T v2)                                    \
    {                                                                    \
        return T(uint32_t(v1) + uint32_t(v2));                           \
    }                                                                    \
    constexpr T operator-(T v1, T v2)                                    \
    {                                                                    \
        return T(uint32_t(v1) - uint32_t(v2));                           \
    }                                                                    \
    inline T& operator+=(T& v1, T v2) { return v1 = v1 + v2; }           \
    inline T& operator-=(T& v1, T v2) { return v1 = v1 - v2; }           \
    inline T& operator+=(T& v1, uint32_t v2) { return v1 = v1 + T(v2); } \
    inline T& operator-=(T& v1, uint32_t v2) { return v1 = v1 - T(v2); } \
    inline T& operator+=(T& v1, int v2) { return v1 = v1 + T(v2); }      \
    inline T& operator-=(T& v1, int v2) { return v1 = v1 - T(v2); }

#define ENABLE_INCREMENT_OPERATIONS(T)                            \
    inline T& operator++(T& v) { return v = T(uint32_t(v) + 1); } \
    inline T& operator--(T& v) { return v = T(uint32_t(v) - 1); }

ENABLE_BASIC_OPERATIONS(Square)
ENABLE_BASIC_OPERATIONS(Rank)
ENABLE_BASIC_OPERATIONS(File)
ENABLE_BASIC_OPERATIONS(Piece)
ENABLE_BASIC_OPERATIONS(PieceKind)

ENABLE_INCREMENT_OPERATIONS(Square)
ENABLE_INCREMENT_OPERATIONS(Rank)
ENABLE_INCREMENT_OPERATIONS(File)
ENABLE_INCREMENT_OPERATIONS(PieceKind)
ENABLE_INCREMENT_OPERATIONS(Piece)

constexpr Castling operator&(Castling c1, Castling c2)
{
    return Castling(uint32_t(c1) & uint32_t(c2));
}
inline Castling& operator|=(Castling& c1, Castling c2)
{
    return c1 = Castling(c1 | c2);
}
inline Castling& operator&=(Castling& c1, Castling c2)
{
    return c1 = Castling(c1 & c2);
}

constexpr Castling operator!(Castling castling)
{
    return Castling(~castling);
}

constexpr Bitboard square_bb(Square sq)
{
    return 1ULL << sq;
}

constexpr PieceKind make_piece_kind(Piece piece)
{
    if (piece == NO_PIECE) return NO_PIECE_KIND;
    return PieceKind((piece - 1) % 6 + 1);
}

constexpr Piece make_piece(Color side, PieceKind piece_kind)
{
    if (piece_kind == NO_PIECE_KIND) return NO_PIECE;
    return Piece(piece_kind + 6 * side);
}

constexpr Color operator!(Color color)
{
    return Color(1 - color);
}

constexpr Color get_color(Piece piece)
{
    ASSERT(piece != NO_PIECE);
    return (piece < B_PAWN) ? WHITE : BLACK;
}

constexpr PieceKind get_piece_kind(Piece piece)
{
    ASSERT(piece != NO_PIECE);
    return PieceKind((piece - 1) % 6 + 1);
}

constexpr Piece piece_flip_color(Piece piece, bool doFlip)
{
    return doFlip
         ? make_piece(!get_color(piece), get_piece_kind(piece))
         : piece;
}

constexpr bool is_piece_slider(Piece piece)
{
    PieceKind piece_kind = get_piece_kind(piece);
    return piece_kind == BISHOP || piece_kind == ROOK || piece_kind == QUEEN;
}

inline int distance(Square from, Square to)
{
    return std::max(abs(int(rank(from)) - int(rank(to))),
                    abs(int(file(from)) - int(file(to))));
}

std::ostream& print_bitboard(std::ostream& stream, Bitboard bb);

std::string sq2str(Square sq);

struct Limits
{
    Limits()
        : searchmovesnum(0),
          ponder(false),
          movestogo(0),
          depth(0),
          nodes(0),
          movetime(0),
          infinite(false)
    {
        timeleft[WHITE] = timeleft[BLACK] = 0;
        timeinc[WHITE] = timeinc[BLACK] = 0;
    }

    Move searchmoves[MAX_MOVES];
    int searchmovesnum;
    bool ponder;
    int timeleft[COLOR_NUM];
    int timeinc[COLOR_NUM];
    int movestogo;
    int depth;
    int64_t nodes;
    int mate;
    int movetime;
    bool infinite;
};

/* PCV
 * Each 4 bits represent single Piece count.
 * In total 10 * 4 = 40 bits are used.
 * Order (from least singnifcant bits to most)
 * is the same as order in Piece enum.
 * First 4 bits are not used (represent NO_PIECE).
 */
using PieceCountVector = uint64_t;

#define C(x) static_cast<uint64_t>((x))

constexpr PieceCountVector create_pcv(int wp, int wn, int wb, int wr, int wq,
                                      int bp, int bn, int bb, int br, int bq)
{
    return (C(wp) << 4LL) | (C(wn) << 8LL) | (C(wb) << 12LL) | (C(wr) << 16LL) |
           (C(wq) << 20LL) | (C(bp) << 28LL) | (C(bn) << 32LL) |
           (C(bb) << 36LL) | (C(br) << 40LL) | (C(bq) << 44LL);
}

template <Piece piece>
constexpr int get_count_pcv(PieceCountVector pcv)
{
    ASSERT(piece != NO_PIECE && piece != W_KING && piece != B_KING);
    return static_cast<int>((pcv >> C(4 * piece)) & 0xF);
}

constexpr int get_count_pcv(PieceCountVector pcv, Piece piece)
{
    ASSERT(piece != NO_PIECE && piece != W_KING && piece != B_KING);
    return static_cast<int>((pcv >> C(4 * piece)) & 0xF);
}

template <Piece piece>
constexpr PieceCountVector modify_pcv(PieceCountVector pcv, int c)
{
    ASSERT(piece != NO_PIECE && piece != W_KING && piece != B_KING);
    return (pcv & ~(C(0xF) << C(4 * piece))) | (C(c) << C(4 * piece));
}

constexpr PieceCountVector modify_pcv(PieceCountVector pcv, int c, Piece piece)
{
    ASSERT(piece != NO_PIECE && piece != W_KING && piece != B_KING);
    return (pcv & ~(C(0xF) << C(4 * piece))) | (C(c) << C(4 * piece));
}

#undef C

}  // namespace engine

#endif  // CHESS_ENGINE_TYPES_H_
