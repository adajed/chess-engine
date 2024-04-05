#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>
#include "types.h"
#include "bitboard.h"
#include "bithacks.h"
#include "position_bitboards.h"
#include "positions.h"
#include "zobrist_hash.h"

#include <functional>
#include <gtest/internal/gtest-param-util.h>
#include <map>
#include <gtest/gtest_pred_impl.h>
#include <vector>

using namespace engine;

// check if only squares that are in bb satisfy pred
// (i.e. pred(x, y) <=> (x, y) in bb)
void check_bb(Bitboard bb, std::function<bool(int, int)> pred)
{
    for (int rank = 0; rank < 8; ++rank)
    {
        for (int file = 0; file < 8; ++file)
        {
            EXPECT_EQ(pred(rank, file), bool(bb & square_bb(make_square(Rank(rank), File(file))))) << rank << "," << file;
        }
    }
}

Bitboard bb_from_function(std::function<bool(int, int)> pred)
{
    Bitboard bb = 0ULL;
    for (int rank = 0; rank < 8; ++rank)
    {
        for (int file = 0; file < 8; ++file)
        {
            Square sq = make_square(Rank(rank), File(file));
            if (pred(rank, file))
                bb |= square_bb(sq);
        }
    }

    return bb;
}

TEST(Bitboard, ranks_bb)
{
    for (int rank = 0; rank < 8; rank++)
    {
        check_bb(RANKS_BB[rank], [rank](int x, int /* y */){ return x == rank; });
    }
}

TEST(Bitboard, files_bb)
{
    for (int file = 0; file < 8; file++)
    {
        check_bb(FILES_BB[file], [file](int /* x */, int y){ return y == file; });
    }
}

TEST(Bitboard, opponent_ranks_bb)
{
    check_bb(OPPONENT_RANKS_BB[WHITE], [](int x, int /* y */){ return x >= 4; });
    check_bb(OPPONENT_RANKS_BB[BLACK], [](int x, int /* y */){ return x < 4; });
}

TEST(Bitboard, middle_ranks_bb)
{
    check_bb(middle_ranks_bb, [](int x, int /* y */){ return x > 0 && x < 7; });
}

TEST(Bitboard, center_bb)
{
    check_bb(center_bb, [](int x, int y){ return x >= 3 && x <= 4 && y >= 3 && y <= 4; });
    check_bb(opponents_center_bb[WHITE], [](int x, int y){ return x >= 4 && x <= 5 && y >= 2 && y <= 5; });
    check_bb(opponents_center_bb[BLACK], [](int x, int y){ return x >= 2 && x <= 3 && y >= 2 && y <= 5; });
}

TEST(Bitboard, color_squares_bb)
{
    check_bb(color_squares_bb[WHITE], [](int x, int y){ return (x + y) % 2 == 1; });
    check_bb(color_squares_bb[BLACK], [](int x, int y){ return (x + y) % 2 == 0; });
}


TEST(Bitboard, forward_ranks_bb)
{
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            Square sq = make_square(Rank(rank), File(file));
            check_bb(forward_ranks_bb<WHITE>(sq), [rank](int x, int /* y */){return x > rank;});
            check_bb(forward_ranks_bb<BLACK>(sq), [rank](int x, int /* y */){return x < rank;});
        }
    }
}

TEST(Bitboard, passed_pawn_bb)
{
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            Square sq = make_square(Rank(rank), File(file));

            auto f_white = [rank, file](int x, int y) {
                return x > rank && abs(y - file) <= 1;
            };
            check_bb(passed_pawn_bb<WHITE>(sq), f_white);

            auto f_black = [rank, file](int x, int y) {
                return x < rank && abs(y - file) <= 1;
            };
            check_bb(passed_pawn_bb<BLACK>(sq), f_black);
        }
    }
}

TEST(Bitboard, squares_left_behind_bb)
{
    for (int rank = 0; rank < 8; rank++)
    {
        for (int file = 0; file < 8; file++)
        {
            Square sq = make_square(Rank(rank), File(file));

            auto f_white = [rank, file](int x, int y) {
                return x <= rank && abs(y - file) == 1;
            };
            check_bb(squares_left_behind_bb<WHITE>(sq), f_white);

            auto f_black = [rank, file](int x, int y) {
                return x >= rank && abs(y - file) == 1;
            };
            check_bb(squares_left_behind_bb<BLACK>(sq), f_black);
        }
    }
}

TEST(Bitboard, get_outposts)
{
    for (const auto& fen : test_positions)
    {
        Position position(fen);

        auto f_white = [&position](int x, int y) {
            Square sq = make_square(Rank(x), File(y));
            Bitboard sq_bb = square_bb(sq);

            if (position.piece_at(sq) == W_PAWN || position.piece_at(sq) == B_PAWN)
                return false;

            if (position.pieces(WHITE, PAWN) & (shift<SOUTHEAST>(sq_bb) | shift<SOUTHWEST>(sq_bb)))
            {
                Bitboard bb = NEIGHBOUR_FILES_BB[File(y)] & forward_ranks_bb<WHITE>(sq);
                return (position.pieces(BLACK, PAWN) & bb) ? false : true;
            }

            return false;
        };


        check_bb(get_outposts<WHITE>(position), f_white);

        auto f_black = [&position](int x, int y) {
            Square sq = make_square(Rank(x), File(y));
            Bitboard sq_bb = square_bb(sq);

            if (position.piece_at(sq) == W_PAWN || position.piece_at(sq) == B_PAWN)
                return false;

            if (position.pieces(BLACK, PAWN) & (shift<NORTHEAST>(sq_bb) | shift<NORTHWEST>(sq_bb)))
            {
                Bitboard bb = NEIGHBOUR_FILES_BB[File(y)] & forward_ranks_bb<BLACK>(sq);
                return (position.pieces(WHITE, PAWN) & bb) ? false : true;
            }

            return false;
        };

        check_bb(get_outposts<BLACK>(position), f_black);
    }
}

TEST(Bitboard, backward_pawns)
{
    for (auto fen : test_positions) {
        Position position(fen);

        Bitboard wPawns = position.pieces(WHITE, PAWN);
        Bitboard bPawns = position.pieces(BLACK, PAWN);

        auto f_white = [&position](int x, int y) {
            if (x == 7)
                return false;

            Square sq = make_square(Rank(x), File(y));
            Bitboard nextSq = square_bb(make_square(Rank(x + 1), File(y)));

            if (position.piece_at(sq) != W_PAWN)
                return false;
            if (pawn_attacks<WHITE>(position.pieces(WHITE, PAWN)) & nextSq)
                return false;
            if (!(pawn_attacks<BLACK>(position.pieces(BLACK, PAWN)) & nextSq))
                return false;
            return true;
        };

        check_bb(backward_pawns<WHITE>(wPawns, bPawns), f_white);

        auto f_black = [&position](int x, int y) {
            if (x == 0)
                return false;

            Square sq = make_square(Rank(x), File(y));
            Bitboard nextSq = square_bb(make_square(Rank(x - 1), File(y)));

            if (position.piece_at(sq) != B_PAWN)
                return false;
            if (pawn_attacks<BLACK>(position.pieces(BLACK, PAWN)) & nextSq)
                return false;
            if (!(pawn_attacks<WHITE>(position.pieces(WHITE, PAWN)) & nextSq))
                return false;
            return true;
        };

        check_bb(backward_pawns<BLACK>(bPawns, wPawns), f_black);
    }

}

TEST(Bitboard, pseudoattacks_ROOK)
{
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
    {
        auto f = [sq](int x, int y)
        {
            if (rank(sq) == x && file(sq) == y)
                return false;
            return (x == rank(sq) || y == file(sq));
        };

        check_bb(pseudoattacks<ROOK>(sq), f);
    }
}

TEST(Bitboard, pseudoattacks_BISHOP)
{
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
    {
        auto f = [sq](int x, int y)
        {
            if (rank(sq) == x && file(sq) == y)
                return false;

            int c1 = static_cast<int>(rank(sq)) - static_cast<int>(file(sq));
            int c2 = static_cast<int>(rank(sq)) + static_cast<int>(file(sq));
            return ((x - y) == c1 || (x + y) == c2);
        };

        check_bb(pseudoattacks<BISHOP>(sq), f);
    }
}


TEST(Bitboard, pseudoattacks_QUEEN)
{
    for (Square sq = SQ_A1; sq <= SQ_H8; ++sq)
    {
        auto f = [sq](int x, int y)
        {
            if (rank(sq) == x && file(sq) == y)
                return false;

            int c1 = static_cast<int>(rank(sq)) - static_cast<int>(file(sq));
            int c2 = static_cast<int>(rank(sq)) + static_cast<int>(file(sq));
            return ((x - y) == c1 || (x + y) == c2 || x == rank(sq) || y == file(sq));
        };

        check_bb(pseudoattacks<QUEEN>(sq), f);
    }
}

TEST(Bitboard, lines)
{
    for (Square from = SQ_A1; from <= SQ_H8; ++from)
    {
        for (Square to = SQ_A1; to <= SQ_H8; ++to)
        {
            auto fun = [from, to](int r, int f)
            {
                int r_from = static_cast<int>(rank(from));
                int r_to = static_cast<int>(rank(to));
                int f_from = static_cast<int>(file(from));
                int f_to = static_cast<int>(file(to));

                if (r_from == r_to)
                    return r == r_from && (std::min(f_from, f_to) <= f && f <= std::max(f_from, f_to));

                if (f_from == f_to)
                    return f == f_from && (std::min(r_from, r_to) <= r && r <= std::max(r_from, r_to));

                if ((f_from - r_from) == (f_to - r_to))
                    return (f - r) == (f_to - r_to) && (std::min(r_from, r_to) <= r && r <= std::max(r_from, r_to));

                if ((f_from + r_from) == (f_to + r_to))
                    return (f + r) == (f_to + r_to) && (std::min(r_from, r_to) <= r && r <= std::max(r_from, r_to));

                return false;
            };
            check_bb(LINES[from][to], fun);
        }
    }
}

TEST(Bitboard, full_lines)
{
    for (Square from = SQ_A1; from <= SQ_H8; ++from)
    {
        for (Square to = SQ_A1; to <= SQ_H8; ++to)
        {
            auto f = [from, to](int r, int f)
            {
                int r_from = static_cast<int>(rank(from));
                int r_to = static_cast<int>(rank(to));
                int f_from = static_cast<int>(file(from));
                int f_to = static_cast<int>(file(to));

                if (from == to)
                    return false;

                if (r_from == r_to)
                    return r == r_from;

                if (f_from == f_to)
                    return f == f_from;

                if ((f_from - r_from) == (f_to - r_to))
                    return (f - r) == (f_to - r_to);

                if ((f_from + r_from) == (f_to + r_to))
                    return (f + r) == (f_to + r_to);

                return false;
            };

            check_bb(FULL_LINES[from][to], f);
        }
    }
}


const std::map<Direction, std::tuple<int, int, std::string>> DIRECTION_MAP = {
    {NORTH,         { 1,  0, "north"}},
    {SOUTH,         {-1,  0, "south"}},
    {EAST,          { 0,  1, "east"}},
    {WEST,          { 0, -1, "west"}},
    {NORTHWEST,     { 1, -1, "northwest"}},
    {NORTHEAST,     { 1,  1, "northeast"}},
    {SOUTHWEST,     {-1, -1, "southwest"}},
    {SOUTHEAST,     {-1,  1, "southeast"}},
    {DOUBLENORTH,   { 2,  0, "doublenorth"}},
    {DOUBLESOUTH,   {-2,  0, "doublesouth"}}
};

const std::vector<Bitboard> testCases = {
    all_squares_bb,
    ~all_squares_bb,
    white_squares_bb,
    black_squares_bb,

    rank1_bb,
    rank2_bb,
    rank3_bb,
    rank4_bb,
    rank5_bb,
    rank6_bb,
    rank7_bb,
    rank8_bb,

    fileA_bb,
    fileB_bb,
    fileC_bb,
    fileD_bb,
    fileE_bb,
    fileF_bb,
    fileG_bb,
    fileH_bb,

    NEIGHBOUR_FILES_BB[0],
    NEIGHBOUR_FILES_BB[1],
    NEIGHBOUR_FILES_BB[2],
    NEIGHBOUR_FILES_BB[3],
    NEIGHBOUR_FILES_BB[4],
    NEIGHBOUR_FILES_BB[5],
    NEIGHBOUR_FILES_BB[6],
    NEIGHBOUR_FILES_BB[7],

    middle_ranks_bb,
};

const std::vector<Direction> ALL_DIRECTIONS = {
    NORTH, SOUTH, EAST, WEST,
    NORTHEAST, NORTHWEST, SOUTHEAST, SOUTHWEST,
    DOUBLENORTH, DOUBLESOUTH
};

class ShiftTest : public testing::TestWithParam<Direction> {};

TEST_P(ShiftTest, shift)
{
    Direction dir = GetParam();

    for (Bitboard bb : testCases)
    {
        auto f = [bb, dir](int x, int y) {
            int px = x - std::get<0>(DIRECTION_MAP.at(dir));
            int py = y - std::get<1>(DIRECTION_MAP.at(dir));

            if (px >= 0 && px < 8 && py >= 0 && py < 8)
                return bool(bb & square_bb(make_square(Rank(px), File(py))));

            return false;
        };
        check_bb(shift(bb, dir), f);
    }
}
INSTANTIATE_TEST_SUITE_P(Test, ShiftTest,
                        testing::ValuesIn(ALL_DIRECTIONS),
                        [](const testing::TestParamInfo<ShiftTest::ParamType>& info){
                            return std::get<2>(DIRECTION_MAP.at(info.param));
                        });
