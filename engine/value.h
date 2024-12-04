#ifndef CHESS_ENGINE_VALUE_H_
#define CHESS_ENGINE_VALUE_H_

#include "types.h"
#include <cstdint>
#include <iomanip>

namespace engine
{

using Value = int64_t;

struct Score
{
    Value mg, eg;

    constexpr Score() : mg(0), eg(0) {}
    constexpr Score(Value mg, Value eg) : mg(mg), eg(eg) {}
    constexpr Score(Value v) : mg(v), eg(v) {}
    Score(const Score& other) = default;
    Score(Score&& other) = default;

    constexpr Score& operator=(const Score& other) = default;
    constexpr Score& operator=(Score&& other) = default;

    constexpr Score operator+(const Score& other) const
    {
        return Score(mg + other.mg, eg + other.eg);
    }
    constexpr Score operator-(const Score& other) const
    {
        return Score(mg - other.mg, eg - other.eg);
    }
    constexpr Score operator*(const Score& other) const
    {
        return Score(mg * other.mg, eg * other.eg);
    }
    constexpr Score operator/(const Score& other) const
    {
        return Score(mg / other.mg, eg / other.eg);
    }

    constexpr Score& operator+=(const Score& other)
    {
        mg += other.mg;
        eg += other.eg;

        return *this;
    }
};

constexpr Score operator*(const Value& value, const Score& score)
{
    return Score(score.mg * value, score.eg * value);
}

inline std::ostream& operator<< (std::ostream& os, Score s)
{
    return os << std::setw(6) << std::setfill(' ') << s.mg << " "
              << std::setw(6) << std::setfill(' ') << s.eg << " ";
}

#define S(mg, eg) (Score((mg), (eg)))

constexpr Score PIECE_VALUE[PIECE_KIND_NUM] = {
    //     ,       pawn ,     knight ,     bishop ,         rook ,        queen ,     king
    S(0, 0), S(300, 370), S(890, 880), S(900, 950), S(1400, 1550), S(2900, 2800), S(0, 0)};


constexpr Value VALUE_ALL_PIECES = 2 * (8 * PIECE_VALUE[PAWN] +
                                       2 * PIECE_VALUE[KNIGHT] +
                                       2 * PIECE_VALUE[BISHOP] +
                                       2 * PIECE_VALUE[ROOK] +
                                       1 * PIECE_VALUE[QUEEN]).eg;

constexpr Value VALUE_DRAW = 0LL;
constexpr Value VALUE_POSITIVE_DRAW = 10LL;
constexpr Value VALUE_NONE = 640'002LL;
constexpr Value VALUE_INFINITE = VALUE_NONE - 1;
constexpr Value VALUE_MATE = VALUE_INFINITE - 1;
constexpr Value VALUE_KNOWN_WIN = VALUE_MATE - 8 * MAX_DEPTH - 8 * VALUE_ALL_PIECES;

static_assert(VALUE_ALL_PIECES < VALUE_KNOWN_WIN);

/**
 * @brief Value corresponding to known mate (win) in given ply.
 */
constexpr Value win_in(int ply)
{
    return VALUE_MATE - ply;
}

/**
 * @brief Value corresponding to known mate (loss) in given ply.
 */
constexpr Value lost_in(int ply)
{
    return -win_in(ply);
}

constexpr bool is_mate(Value score)
{
    ASSERT(score != VALUE_NONE && score != VALUE_INFINITE && score != -VALUE_INFINITE);
    return score <= lost_in(MAX_DEPTH) || score >= win_in(MAX_DEPTH);
}

constexpr Score MOBILITY_BONUS[PIECE_KIND_NUM] = {
    //           pawn,   knight,  bishop,     rook,   queen,    king
    S(0, 0), S(5, 10), S(12, 24), S(18, 8), S(6, 24), S(4, 12), S(0, 10)};

constexpr Score PINNED_PENALTY[PIECE_KIND_NUM] = {
    //              pawn,      knight,      bishop,    rook,   queen,    king
    S(0, 0), S(-20, -20), S(-10, -15), S(-10, -20), S(0, 0), S(0, 0), S(0, 0)};

// bonus for rook on semiopen file
constexpr Score ROOK_SEMIOPEN_FILE_BONUS = S(10, 11);

// bonus for rook on open file
constexpr Score ROOK_OPEN_FILE_BONUS = S(20, 40);

constexpr Score TRAPPED_ROOK_PENALTY = S(-50, -10);

// bonus for bishop pair
constexpr Score BISHOP_PAIR_BONUS = S(50, 60);

// bonus for connecting rooks
constexpr Score CONNECTED_ROOKS_BONUS = S(20, 10);

constexpr Score OUTPOST_KNIGHT_BONUS = S(25, 10);

constexpr Score OUTPOST_BISHOP_BONUS = S(20, 10);

// bonus for pawns controlling center
constexpr Score PAWN_CONTROL_CENTER_BONUS = S(30, 30);

// bonus for passed pawn
constexpr Score PASSED_PAWN_BONUS = S(20, 40);
constexpr Value PASSED_PAWN_RANK_WEIGHT[RANK_NUM] = {
    // 0, 1, 2, 3, 4, 5,  6, 7
       0, 1, 1, 2, 3, 6, 10, 0};

// penalty for double pawns
constexpr Score DOUBLE_PAWN_PENALTY = S(-15, -45);

constexpr Value CONNECTED_PAWNS_BONUS[RANK_NUM] = {
    0, 0, 2, 5, 20, 40, 80, 0};

constexpr Score BACKWARD_PAWN_PENALTY = S(-30, -100);

constexpr Score ISOLATED_PAWN_PENALTY = S(-20, -80);

constexpr Score KING_SAFETY_BONUS = S(30, 0);
constexpr Score SAFE_KNIGHT = S(10, 2);
constexpr Score CONTROL_CENTER_KNIGHT = S(10, 10);

constexpr Score CONTROL_SPACE[PIECE_KIND_NUM] = {
    //            pawn,  knight,   bishop,      rook,     queen,    king
    S(0, 0), S(20, 30), S(20, 0), S(10, 5), S(10, 10), S(10, 20), S(0, 0)};

constexpr Score KING_PROTECTOR_PENALTY[PIECE_KIND_NUM] = {
    //          pawn,    knight,    bishop,    rook,   queen,    king
    S(0, 0), S(0, 0), S(-6, -4), S(-5, -3), S(0, 0), S(0, 0), S(0, 0)};

constexpr Score KING_ATTACKER_PENALTY[PIECE_KIND_NUM] = {
    //          pawn,    knight,    bishop,    rook,   queen,    king
    S(0, 0), S(0, 0), S(-7, -4), S(-4, -3), S(0, 0), S(0, 0), S(0, 0)};

constexpr Score PAWN_ISLAND_PENALTY = S(-10, -20);

constexpr Score VULNERABLE_QUEEN_PENALTY = S(-30, -15);

constexpr Score WEAK_BACKRANK_PENALTY = S(-75, -100);

constexpr Score NOT_CENTRALIZED_KING_PENALTY = S(0, -100);

constexpr Score WEAK_KING_DIAGONALS = S(-5, 0);
constexpr Score WEAK_KING_LINES = S(-7, 0);

constexpr Score KING_PAWN_PROXIMITY_PENALTY = S(0, -5);

constexpr Score PAWNS_ON_SAME_COLOR_AS_BISHOP_PENALTY = S(-3, -5);

#undef S


} /* namespace engine */

#endif /* end of include guard: CHESS_ENGINE_VALUE_H_ */
