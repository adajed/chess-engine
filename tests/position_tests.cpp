#if defined(_WIN32)
#include <stdint.h>
#else
#include <bits/stdint-uintn.h>
#endif
#include <gtest/gtest.h>

#include "bitboard.h"
#include "position.h"
#include "score.h"

using namespace engine;

namespace
{

void validate(const Position& position)
{
    for (Piece piece = W_PAWN; piece <= B_KING; ++piece)
    {
        Bitboard piece_bb = position.pieces(get_color(piece), get_piece_kind(piece));
        Bitboard bb = 0ULL;

        for (int i = 0; i < position.no_pieces(piece); ++i)
        {
            Square square = position.piece_position(piece, i);
            EXPECT_EQ(bb & square_bb(square), 0ULL);
            bb |= square_bb(square);
        }

        EXPECT_EQ(piece_bb, bb);
    }

    for (Piece piece1 = W_PAWN; piece1 <= B_KING; ++piece1)
        for (Piece piece2 = W_PAWN; piece2 <= B_KING; ++piece2)
            if (piece1 != piece2)
            {
                Bitboard bb1 = position.pieces(get_color(piece1), get_piece_kind(piece1));
                Bitboard bb2 = position.pieces(get_color(piece2), get_piece_kind(piece2));
                EXPECT_EQ(bb1 & bb2, 0ULL);
            }

    EXPECT_EQ(position.pieces(WHITE) & position.pieces(BLACK), 0ULL);

    for (Color side : {WHITE, BLACK})
    {
        Bitboard bb1 = position.pieces(side);
        Bitboard bb2 = 0ULL;
        for (PieceKind piecekind : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
            bb2 |= position.pieces(side, piecekind);
        EXPECT_EQ(bb1, bb2);
    }

    for (PieceKind piecekind : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
    {
        Bitboard bb1 = position.pieces(piecekind);
        Bitboard bb2 = 0ULL;
        for (Color side : {WHITE, BLACK})
            bb2 |= position.pieces(side, piecekind);
        EXPECT_EQ(bb1, bb2);
    }

    for (Piece piece = W_PAWN; piece <= B_KING; ++piece)
    {
        for (int i = 0; i < position.no_pieces(piece); ++i)
        {
            Square square = position.piece_position(piece, i);
            EXPECT_EQ(position.piece_at(square), piece);
        }
    }

}

TEST(PositionTest, constructor_startpos)
{
    Position position;

    ASSERT_EQ(position.color(), WHITE);
    ASSERT_EQ(position.castling_rights(), ALL_CASTLING);
    ASSERT_EQ(position.enpassant_square(), NO_SQUARE);

    EXPECT_EQ(position.pieces(WHITE, PAWN),   0x000000000000FF00ULL);
    EXPECT_EQ(position.pieces(WHITE, KNIGHT), 0x0000000000000042ULL);
    EXPECT_EQ(position.pieces(WHITE, BISHOP), 0x0000000000000024ULL);
    EXPECT_EQ(position.pieces(WHITE, ROOK),   0x0000000000000081ULL);
    EXPECT_EQ(position.pieces(WHITE, QUEEN),  0x0000000000000008ULL);
    EXPECT_EQ(position.pieces(WHITE, KING),   0x0000000000000010ULL);
    EXPECT_EQ(position.pieces(BLACK, PAWN),   0x00FF000000000000ULL);
    EXPECT_EQ(position.pieces(BLACK, KNIGHT), 0x4200000000000000ULL);
    EXPECT_EQ(position.pieces(BLACK, BISHOP), 0x2400000000000000ULL);
    EXPECT_EQ(position.pieces(BLACK, ROOK),   0x8100000000000000ULL);
    EXPECT_EQ(position.pieces(BLACK, QUEEN),  0x0800000000000000ULL);
    EXPECT_EQ(position.pieces(BLACK, KING),   0x1000000000000000ULL);

    validate(position);

    EXPECT_FALSE(position.is_in_check(WHITE));
    EXPECT_FALSE(position.is_in_check(BLACK));
    EXPECT_FALSE(position.is_checkmate());

    EXPECT_EQ(position.half_moves(), 0);
    EXPECT_EQ(position.ply_count(), 1);
}

TEST(PositionTest, constructor)
{
    std::vector<std::string> fens = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 0 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1",
    };

    for (std::string fen : fens)
    {
        Position position(fen);
        validate(position);

        EXPECT_FALSE(position.is_checkmate());
    }
}

TEST(PositionTest, fen)
{
    std::vector<std::string> fens = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 0 1",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2",
    };

    for (std::string fen : fens)
    {
        Position position(fen);
        EXPECT_EQ(position.fen(), fen);
    }
}

TEST(PositionTest, do_move)
{
    using TestCase = std::tuple<std::string, Move, MoveInfo, std::string>;

    std::vector<TestCase> test_cases = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         create_move(SQ_E2, SQ_E4),
         create_moveinfo(NO_PIECE_KIND, ALL_CASTLING, NO_SQUARE, false, 0),
         "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        },
        {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
         create_move(SQ_G8, SQ_F6),
         create_moveinfo(NO_PIECE_KIND, ALL_CASTLING, SQ_E3, false, 0),
         "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 1 2",
        },
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         create_move(SQ_E5, SQ_G6),
         create_moveinfo(PAWN, ALL_CASTLING, NO_SQUARE, false, 0),
         "r3k2r/p1ppqpb1/bn2pnN1/3P4/1p2P3/2N2Q1p/PPPBBPPP/R3K2R b KQkq - 0 1"
        },
    };

    for (const TestCase& test_case : test_cases)
    {
        Position position(std::get<0>(test_case));
        MoveInfo moveinfo = position.do_move(std::get<1>(test_case));
        EXPECT_EQ(moveinfo, std::get<2>(test_case));
        EXPECT_EQ(position.fen(), std::get<3>(test_case));
    }
}

TEST(PositionTest, threefold_repetition)
{
    using TestCase = std::tuple<std::vector<std::string>, bool>;
    std::vector<TestCase> test_cases = {
        {{"e2e4", "e7e5", "e1e2", "e8e7", "e2e1", "e7e8", "e1e2", "e8e7", "e2e1", "e7e8", "e1e2", "e8e7", "e2e1", "e7e8"}, true}
    };

    for (const TestCase& test_case : test_cases)
    {
        Position position;
        for (const std::string& move : std::get<0>(test_case))
            position.do_move(position.parse_uci(move));
        EXPECT_EQ(position.threefold_repetition(), std::get<1>(test_case));
    }
}

TEST(PositionTest, parse_san)
{
    using TestCase = std::tuple<std::string, std::string, Move>;
    std::vector<TestCase> test_cases = {
        // positive tests
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e4", create_move(SQ_E2, SQ_E4)},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "Nf3", create_move(SQ_G1, SQ_F3)},
        {"6k1/5ppp/8/8/8/8/8/1RK5 w - - 0 1", "Rb8#", create_move(SQ_B1, SQ_B8)},
        {"r5k1/5ppp/8/8/8/8/1R6/1RK5 w - - 0 1", "Rb8+", create_move(SQ_B2, SQ_B8)},
        {"rr4k1/5ppp/8/8/8/2R5/2R5/2RK4 w - - 0 1", "Rc8+", create_move(SQ_C3, SQ_C8)},
        {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", "Qd1+", create_move(SQ_D6, SQ_D1)},
        {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1", "d5", create_move(SQ_D4, SQ_D5)},
        {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 1", "f5", create_move(SQ_F6, SQ_F5)},
        {"rnbqkbnr/ppp2ppp/8/3pp3/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 0 3", "Nbd2", create_move(SQ_B1, SQ_D2)},
        {"rnbqkbnr/ppp2ppp/8/3pp3/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 0 3", "Nfd2", create_move(SQ_F3, SQ_D2)},
        {"r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", "0-0", create_castling(KING_CASTLING)},
        {"rnbqkbnr/ppp2ppp/8/3pp3/2P1P3/8/PP1P1PPP/RNBQKBNR w KQkq - 0 3", "exd5", create_move(SQ_E4, SQ_D5)},
        {"rnbqkbnr/ppp2ppp/8/3pp3/2P1P3/8/PP1P1PPP/RNBQKBNR w KQkq - 0 3", "cxd5", create_move(SQ_C4, SQ_D5)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxa8=N", create_promotion(SQ_B7, SQ_A8, KNIGHT)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxa8=B", create_promotion(SQ_B7, SQ_A8, BISHOP)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxa8=R", create_promotion(SQ_B7, SQ_A8, ROOK)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxa8=Q", create_promotion(SQ_B7, SQ_A8, QUEEN)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "b8=N", create_promotion(SQ_B7, SQ_B8, KNIGHT)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "b8=B", create_promotion(SQ_B7, SQ_B8, BISHOP)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "b8=R", create_promotion(SQ_B7, SQ_B8, ROOK)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "b8=Q", create_promotion(SQ_B7, SQ_B8, QUEEN)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxc8=N+", create_promotion(SQ_B7, SQ_C8, KNIGHT)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxc8=B+", create_promotion(SQ_B7, SQ_C8, BISHOP)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxc8=R+", create_promotion(SQ_B7, SQ_C8, ROOK)},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxc8=Q+", create_promotion(SQ_B7, SQ_C8, QUEEN)},
        {"5k2/8/8/8/8/2R5/1R6/4K3 w - - 0 1", "Rbc2", create_move(SQ_B2, SQ_C2)},
        {"5k2/8/8/8/8/2R5/1R6/4K3 w - - 0 1", "Rcc2", create_move(SQ_C3, SQ_C2)},
        {"5k2/8/8/2N5/8/2N5/8/4K3 w - - 0 1", "N3e4", create_move(SQ_C3, SQ_E4)},
        {"5k2/8/8/2N5/8/2N5/8/4K3 w - - 0 1", "N5e4", create_move(SQ_C5, SQ_E4)},
        {"5k2/8/8/2N5/8/2N5/8/4K3 w - - 0 1", "Nc3e4", create_move(SQ_C3, SQ_E4)},
        {"5k2/8/8/2N5/8/2N5/8/4K3 w - - 0 1", "Nc5e4", create_move(SQ_C5, SQ_E4)},
        {"r3k2r/pppq1ppp/2npbn2/2b1p3/2B1P3/2NPBN2/PPPQ1PPP/R3K2R w KQkq - 4 8", "0-0", KING_CASTLING_MOVE},
        {"r3k2r/pppq1ppp/2npbn2/2b1p3/2B1P3/2NPBN2/PPPQ1PPP/R3K2R w KQkq - 4 8", "0-0-0", QUEEN_CASTLING_MOVE},
        {"r3k2r/pppq1ppp/2npbn2/2b1p3/2B1P3/2NPBN2/PPPQ1PPP/R4RK1 b kq - 5 8", "0-0", KING_CASTLING_MOVE},
        {"r3k2r/pppq1ppp/2npbn2/2b1p3/2B1P3/2NPBN2/PPPQ1PPP/R4RK1 b kq - 5 8", "0-0-0", QUEEN_CASTLING_MOVE},

        // negative tests
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e5", NO_MOVE},
        {"rnbqkbnr/ppp2ppp/8/3pp3/3P4/5N2/PPP1PPPP/RNBQKB1R w KQkq - 0 3", "Nd2", NO_MOVE},
        {"r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", "0-0-0", NO_MOVE},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxa8=P", NO_MOVE},
        { "r1b1kbnr/pPpp1ppp/4pq2/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1", "bxa8=K", NO_MOVE},
        {"rnbqkbnr/ppp2ppp/8/3pp3/2P1P3/8/PP1P1PPP/RNBQKBNR w KQkq - 0 3", "bxd5", NO_MOVE},
        {"5k2/8/8/2N5/8/2N5/8/4K3 w - - 0 1", "Ne4", NO_MOVE},
        {"5k2/8/8/2N5/8/2N5/8/4K3 w - - 0 1", "Nce4", NO_MOVE},
        {"r4rk1/pppq1ppp/2npbn2/2b1p3/2B1P3/2NPBN2/PPPQ1PPP/R4RK1 w - - 6 9", "0-0", NO_MOVE},
        {"r4rk1/pppq1ppp/2npbn2/2b1p3/2B1P3/2NPBN2/PPPQ1PPP/R4RK1 w - - 6 9", "0-0-0", NO_MOVE},
    };

    for (const TestCase& test_case : test_cases)
    {
        Position position(std::get<0>(test_case));
        EXPECT_EQ(position.parse_san(std::get<1>(test_case)), std::get<2>(test_case))
            << "FEN: \"" << std::get<0>(test_case) << "\" SAN: \"" << std::get<1>(test_case) << "\"";
    }
}

TEST(PositionTest, hash)
{
    using TestCase = std::tuple<std::string, Move>;

    std::vector<TestCase> test_cases = {
        {"6k1/5ppp/8/8/8/8/8/1RK5 w - - 0 1", create_move(SQ_B1, SQ_B8)},
        {"r5k1/5ppp/8/8/8/8/1R6/1RK5 w - - 0 1", create_move(SQ_B2, SQ_B8)},
        {"rr4k1/5ppp/8/8/8/2R5/2R5/2RK4 w - - 0 1", create_move(SQ_C3, SQ_C8)},
        {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", create_move(SQ_D6, SQ_D1)},
        {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1", create_move(SQ_D4, SQ_D5)},
        {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 1", create_move(SQ_F6, SQ_F5)},
        {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - 0 1", create_move(SQ_E5, SQ_E6)},
        /* {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1", create_move(SQ_D5, SQ_A4)}, */
        {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - - 0 1", create_move(SQ_G5, SQ_G6)},
        {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - - 0 1", create_move(SQ_H5, SQ_F6)},
        {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - - 0 1", create_move(SQ_F4, SQ_F5)},
        {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - - 0 1", create_move(SQ_F4, SQ_F5)},
        {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 0 1", create_move(SQ_C6, SQ_E5)},
        {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - - 0 1", create_move(SQ_F2, SQ_F4)},
        {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - 0 1", create_move(SQ_D7, SQ_F5)},
        {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - - 0 1", create_move(SQ_B2, SQ_B4)},
        {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 1", create_move(SQ_G4, SQ_G7)},
        {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq - 0 1", create_move(SQ_D2, SQ_E4)},
        {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - - 0 1", create_move(SQ_H7, SQ_H5)},
        {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - - 0 1", create_move(SQ_C5, SQ_B3)},
        {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - - 0 1", create_move(SQ_E8, SQ_E4)},
        /* {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - - 0 1", create_move(SQ_G2, SQ_G4)}, */
        {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - - 0 1", create_move(SQ_F5, SQ_H6)},
        {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - 0 1", create_move(SQ_F7, SQ_F6)},
        {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - - 0 1", create_move(SQ_F2, SQ_F4)},
        {"rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/2N1P3/PP3PPP/R1BQKBNR w KQkq - 0 1", create_move(SQ_D1, SQ_B3)},
        {"rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/1QN1P3/PP3PPP/R1B1KBNR b KQkq - 1 1", create_move(SQ_F5, SQ_C8)},
        {"r1bqk2r/ppp2ppp/2n5/4P3/2Bp2n1/5N1P/PP1N1PP1/R2Q1RK1 b kq - 1 10", create_move(SQ_G4, SQ_H6)},
        {"r1bqrnk1/pp2bp1p/2p2np1/3p2B1/3P4/2NBPN2/PPQ2PPP/1R3RK1 w - - 1 12", create_move(SQ_B2, SQ_B4)},
        {"rnbqkb1r/ppp1pppp/5n2/8/3PP3/2N5/PP3PPP/R1BQKBNR b KQkq - 3 5", create_move(SQ_E7, SQ_E5)},
        /* {"rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ - 1 5", create_move(SQ_D2, SQ_C3)}, */
        {"r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 12", create_move(SQ_F8, SQ_B8)},
        {"rn1qkb1r/pb1p1ppp/1p2pn2/2p5/2PP4/5NP1/PP2PPBP/RNBQK2R w KQkq c6 1 6", create_move(SQ_D4, SQ_D5)},
        {"r1bq1rk1/1pp2pbp/p1np1np1/3Pp3/2P1P3/2N1BP2/PP4PP/R1NQKB1R b KQ - 1 9", create_move(SQ_C6, SQ_D4)},
        {"rnbqr1k1/1p3pbp/p2p1np1/2pP4/4P3/2N5/PP1NBPPP/R1BQ1RK1 w - - 1 11", create_move(SQ_A2, SQ_A4)},
        {"rnbqkb1r/pppp1ppp/5n2/4p3/4PP2/2N5/PPPP2PP/R1BQKBNR b KQkq f3 1 3", create_move(SQ_D7, SQ_D5)},
        {"r1bqk1nr/pppnbppp/3p4/8/2BNP3/8/PPP2PPP/RNBQK2R w KQkq - 2 6", create_move(SQ_C4, SQ_F7)},
        {"rnbq1b1r/ppp2kpp/3p1n2/8/3PP3/8/PPP2PPP/RNBQKB1R b KQ d3 1 5", create_move(SQ_F6, SQ_E4)},
        {"rnbqkb1r/pppp1ppp/3n4/8/2BQ4/5N2/PPP2PPP/RNB2RK1 b kq - 1 6", create_move(SQ_D6, SQ_C4)},
        {"r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 w - - 1 12", create_move(SQ_E5, SQ_F6)},
        {"r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - 2 7", create_move(SQ_D7, SQ_D5)},
        {"r2qkbnr/2p2pp1/p1pp4/4p2p/4P1b1/5N1P/PPPP1PP1/RNBQ1RK1 w kq - 1 8", create_move(SQ_D2, SQ_D4)},
        /* {"1rbq1rk1/p1b1nppp/1p2p3/8/1B1pN3/P2B4/1P3PPP/2RQ1R1K w - - 0 1", create_move(SQ_G4, SQ_F6)}, */
        {"3r2k1/p2r1p1p/1p2p1p1/q4n2/3P4/PQ5P/1P1RNPP1/3R2K1 b - - 0 1", create_move(SQ_F5, SQ_D4)},
        {"r1b1r1k1/1ppn1p1p/3pnqp1/8/p1P1P3/5P2/PbNQNBPP/1R2RB1K w - - 0 1", create_move(SQ_B1, SQ_B2)},
        {"2r4k/pB4bp/1p4p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - - 0 1", create_move(SQ_G5, SQ_C1)},
        {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1", create_move(SQ_B7, SQ_E4)},
        {"5r1k/6pp/1n2Q3/4p3/8/7P/PP4PK/R1B1q3 b - - 0 1", create_move(SQ_H7, SQ_H6)},
        {"r3k2r/pbn2ppp/8/1P1pP3/P1qP4/5B2/3Q1PPP/R3K2R w KQkq - 0 1", create_move(SQ_F3, SQ_E2)},
        {"3r2k1/ppq2pp1/4p2p/3n3P/3N2P1/2P5/PP2QP2/K2R4 b - - 0 1", create_move(SQ_D5, SQ_C3)},
        {"q3rn1k/2QR4/pp2pp2/8/P1P5/1P4N1/6n1/6K1 w - - 0 1", create_move(SQ_G3, SQ_F5)},
        {"6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 1", create_move(SQ_B6, SQ_D6)},
        {"1r4k1/7p/5np1/3p3n/8/2NB4/7P/3N1RK1 w - - 0 1", create_move(SQ_C3, SQ_D5)},
        {"1r2r1k1/p4p1p/6pB/q7/8/3Q2P1/PbP2PKP/1R3R2 w - - 0 1", create_move(SQ_B1, SQ_B2)},
        {"r2q1r1k/pb3p1p/2n1p2Q/5p2/8/3B2N1/PP3PPP/R3R1K1 w - - 0 1", create_move(SQ_D3, SQ_F5)},
        {"8/4p3/p2p4/2pP4/2P1P3/1P4k1/1P1K4/8 w - - 0 1", create_move(SQ_B2, SQ_B4)},
        {"1r1q1rk1/p1p2pbp/2pp1np1/6B1/4P3/2NQ4/PPP2PPP/3R1RK1 w - - 0 1", create_move(SQ_E4, SQ_E5)},
        {"q4rk1/1n1Qbppp/2p5/1p2p3/1P2P3/2P4P/6P1/2B1NRK1 b - - 0 1", create_move(SQ_A8, SQ_C8)},
        {"r2q1r1k/1b1nN2p/pp3pp1/8/Q7/PP5P/1BP2RPN/7K w - - 0 1", create_move(SQ_A4, SQ_D7)},
        {"8/5p2/pk2p3/4P2p/2b1pP1P/P3P2B/8/7K w - - 0 1", create_move(SQ_H3, SQ_G4)},
        {"8/2k5/4p3/1nb2p2/2K5/8/6B1/8 w - - 0 1", create_move(SQ_C4, SQ_B5)},
        {"1B1b4/7K/1p6/1k6/8/8/8/8 w - - 0 1", create_move(SQ_B8, SQ_A7)},
        {"rn1q1rk1/1b2bppp/1pn1p3/p2pP3/3P4/P2BBN1P/1P1N1PP1/R2Q1RK1 b - - 0 1", create_move(SQ_B7, SQ_A6)},
        {"8/p1ppk1p1/2n2p2/8/4B3/2P1KPP1/1P5P/8 w - - 0 1", create_move(SQ_E4, SQ_C6)},
        {"8/3nk3/3pp3/1B6/8/3PPP2/4K3/8 w - - 0 1", create_move(SQ_B5, SQ_D7)},
    };

    for (const TestCase& test_case : test_cases)
    {
        Position position(std::get<0>(test_case));

        uint64_t hash = position.hash();
        uint64_t pawn_hash = position.pawn_hash();

        MoveInfo moveinfo = position.do_move(std::get<1>(test_case));
        position.undo_move(std::get<1>(test_case), moveinfo);

        EXPECT_EQ(hash, position.hash());
        EXPECT_EQ(pawn_hash, position.pawn_hash());
    }
}

TEST(PositionTest, is_in_check)
{
    using TestCase = std::tuple<std::string, bool, bool>;

    std::vector<TestCase> test_cases = {
        {"6k1/5ppp/8/8/8/8/8/1RK5 w - - 0 1", false, false},
        {"r5k1/5ppp/8/8/8/8/1R6/1RK5 w - - 0 1", false, false},
        {"rr4k1/5ppp/8/8/8/2R5/2R5/2RK4 w - - 0 1", false, false},
        {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", false, false},
        {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1", false, false},
        {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 1", false, false},
        {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - 0 1", false, false},
        {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - - 0 1", false, false},
        {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - - 0 1", false, false},
        {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - - 0 1", false, false},
        {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - - 0 1", false, false},
        {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 0 1", false, false},
        {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - - 0 1", false, false},
        {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - 0 1", false, false},
        {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - - 0 1", false, false},
        {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 1", false, false},
        {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq - 0 1", false, false},
        {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - - 0 1", false, false},
        {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - - 0 1", false, false},
        {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - - 0 1", false, false},
        {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - - 0 1", false, false},
        {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1", false, false},
        {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - 0 1", false, false},
        {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - - 0 1", false, false},
        {"rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/2N1P3/PP3PPP/R1BQKBNR w KQkq - 0 1", false, false},
        {"rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/1QN1P3/PP3PPP/R1B1KBNR b KQkq - 1 1", false, false},
        {"r1bqk2r/ppp2ppp/2n5/4P3/2Bp2n1/5N1P/PP1N1PP1/R2Q1RK1 b kq - 1 10", false, false},
        {"r1bqrnk1/pp2bp1p/2p2np1/3p2B1/3P4/2NBPN2/PPQ2PPP/1R3RK1 w - - 1 12", false, false},
        {"rnbqkb1r/ppp1pppp/5n2/8/3PP3/2N5/PP3PPP/R1BQKBNR b KQkq - 3 5", false, false},
        {"r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 12", false, false},
        {"rn1qkb1r/pb1p1ppp/1p2pn2/2p5/2PP4/5NP1/PP2PPBP/RNBQK2R w KQkq c6 1 6", false, false},
        {"r1bq1rk1/1pp2pbp/p1np1np1/3Pp3/2P1P3/2N1BP2/PP4PP/R1NQKB1R b KQ - 1 9", false, false},
        {"rnbqr1k1/1p3pbp/p2p1np1/2pP4/4P3/2N5/PP1NBPPP/R1BQ1RK1 w - - 1 11", false, false},
        {"rnbqkb1r/pppp1ppp/5n2/4p3/4PP2/2N5/PPPP2PP/R1BQKBNR b KQkq f3 1 3", false, false},
        {"r1bqk1nr/pppnbppp/3p4/8/2BNP3/8/PPP2PPP/RNBQK2R w KQkq - 2 6", false, false},
        {"rnbq1b1r/ppp2kpp/3p1n2/8/3PP3/8/PPP2PPP/RNBQKB1R b KQ d3 1 5", false, false},
        {"rnbqkb1r/pppp1ppp/3n4/8/2BQ4/5N2/PPP2PPP/RNB2RK1 b kq - 1 6", false, false},
        {"r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 w - - 1 12", false, false},
        {"r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - 2 7", false, false},
        {"r2qkbnr/2p2pp1/p1pp4/4p2p/4P1b1/5N1P/PPPP1PP1/RNBQ1RK1 w kq - 1 8", false, false},
        {"3r2k1/p2r1p1p/1p2p1p1/q4n2/3P4/PQ5P/1P1RNPP1/3R2K1 b - - 0 1", false, false},
        {"r1b1r1k1/1ppn1p1p/3pnqp1/8/p1P1P3/5P2/PbNQNBPP/1R2RB1K w - - 0 1", false, false},
        {"2r4k/pB4bp/1p4p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - - 0 1", false, false},
        {"5r1k/6pp/1n2Q3/4p3/8/7P/PP4PK/R1B1q3 b - - 0 1", false, false},
        {"r3k2r/pbn2ppp/8/1P1pP3/P1qP4/5B2/3Q1PPP/R3K2R w KQkq - 0 1", false, false},
        {"3r2k1/ppq2pp1/4p2p/3n3P/3N2P1/2P5/PP2QP2/K2R4 b - - 0 1", false, false},
        {"q3rn1k/2QR4/pp2pp2/8/P1P5/1P4N1/6n1/6K1 w - - 0 1", false, false},
        {"6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 1", false, false},
        {"1r4k1/7p/5np1/3p3n/8/2NB4/7P/3N1RK1 w - - 0 1", false, false},
        {"1r2r1k1/p4p1p/6pB/q7/8/3Q2P1/PbP2PKP/1R3R2 w - - 0 1", false, false},
        {"r2q1r1k/pb3p1p/2n1p2Q/5p2/8/3B2N1/PP3PPP/R3R1K1 w - - 0 1", false, false},
        {"8/4p3/p2p4/2pP4/2P1P3/1P4k1/1P1K4/8 w - - 0 1", false, false},
        {"1r1q1rk1/p1p2pbp/2pp1np1/6B1/4P3/2NQ4/PPP2PPP/3R1RK1 w - - 0 1", false, false},
        {"q4rk1/1n1Qbppp/2p5/1p2p3/1P2P3/2P4P/6P1/2B1NRK1 b - - 0 1", false, false},
        {"r2q1r1k/1b1nN2p/pp3pp1/8/Q7/PP5P/1BP2RPN/7K w - - 0 1", false, false},
        {"8/5p2/pk2p3/4P2p/2b1pP1P/P3P2B/8/7K w - - 0 1", false, false},
        {"8/2k5/4p3/1nb2p2/2K5/8/6B1/8 w - - 0 1", false, false},
        {"1B1b4/7K/1p6/1k6/8/8/8/8 w - - 0 1", false, false},
        {"rn1q1rk1/1b2bppp/1pn1p3/p2pP3/3P4/P2BBN1P/1P1N1PP1/R2Q1RK1 b - - 0 1", false, false},
        {"8/p1ppk1p1/2n2p2/8/4B3/2P1KPP1/1P5P/8 w - - 0 1", false, false},
        {"8/3nk3/3pp3/1B6/8/3PPP2/4K3/8 w - - 0 1", false, false},
    };

    for (const TestCase& test_case : test_cases)
    {
        Position position(std::get<0>(test_case));

        EXPECT_EQ(position.is_in_check(WHITE), std::get<1>(test_case)) << std::get<0>(test_case);
        EXPECT_EQ(position.is_in_check(BLACK), std::get<2>(test_case)) << std::get<0>(test_case);
    }
}

TEST(PositionTest, move_types)
{
    // fen, move, isQuiet, isCapture, isGivingCheck
    using TestCase = std::tuple<std::string, Move, bool, bool, bool>;

    std::vector<TestCase> test_cases = {
        {"6k1/5ppp/8/8/8/8/8/1RK5 w - - 0 1", create_move(SQ_B1, SQ_B8), true, false, true},
        {"r5k1/5ppp/8/8/8/8/1R6/1RK5 w - - 0 1", create_move(SQ_B2, SQ_B8), true, false, true},
        {"rr4k1/5ppp/8/8/8/2R5/2R5/2RK4 w - - 0 1", create_move(SQ_C3, SQ_C8), true, false, true},
        {"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", create_move(SQ_D6, SQ_D1), true, false, true},
        {"3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - - 0 1", create_move(SQ_D4, SQ_D5), true, false, false},
        {"2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - - 0 1", create_move(SQ_F6, SQ_F5), true, false, false},
        {"rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq - 0 1", create_move(SQ_E5, SQ_E6), true, false, false},
        /* {"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1", create_move(SQ_D5, SQ_A4), true, false, false}, */
        {"2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - - 0 1", create_move(SQ_G5, SQ_G6), true, false, false},
        {"1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - - 0 1", create_move(SQ_H5, SQ_F6), true, false, false},
        {"4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - - 0 1", create_move(SQ_F4, SQ_F5), true, false, false},
        {"2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - - 0 1", create_move(SQ_F4, SQ_F5), true, false, false},
        {"3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 0 1", create_move(SQ_C6, SQ_E5), true, false, false},
        {"2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - - 0 1", create_move(SQ_F2, SQ_F4), true, false, false},
        {"r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - - 0 1", create_move(SQ_D7, SQ_F5), true, false, false},
        {"r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - - 0 1", create_move(SQ_B2, SQ_B4), true, false, false},
        {"2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - - 0 1", create_move(SQ_G4, SQ_G7), false, true, true},
        {"r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq - 0 1", create_move(SQ_D2, SQ_E4), true, false, false},
        {"r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - - 0 1", create_move(SQ_H7, SQ_H5), true, false, false},
        {"r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - - 0 1", create_move(SQ_C5, SQ_B3), true, false, false},
        {"3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - - 0 1", create_move(SQ_E8, SQ_E4), false, true, false},
        /* {"r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - - 0 1", create_move(SQ_G2, SQ_G4), true, false, false}, */
        {"3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - - 0 1", create_move(SQ_F5, SQ_H6), true, false, false},
        {"2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1", create_move(SQ_B7, SQ_E4), false, true, false},
        {"r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - 0 1", create_move(SQ_F7, SQ_F6), true, false, false},
        {"r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - - 0 1", create_move(SQ_F2, SQ_F4), true, false, false},
        {"rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/2N1P3/PP3PPP/R1BQKBNR w KQkq - 0 1", create_move(SQ_D1, SQ_B3), true, false, false},
        {"rn1qkb1r/pp2pppp/5n2/3p1b2/3P4/1QN1P3/PP3PPP/R1B1KBNR b KQkq - 1 1", create_move(SQ_F5, SQ_C8), true, false, false},
        {"r1bqk2r/ppp2ppp/2n5/4P3/2Bp2n1/5N1P/PP1N1PP1/R2Q1RK1 b kq - 1 10", create_move(SQ_G4, SQ_H6), true, false, false},
        {"r1bqrnk1/pp2bp1p/2p2np1/3p2B1/3P4/2NBPN2/PPQ2PPP/1R3RK1 w - - 1 12", create_move(SQ_B2, SQ_B4), true, false, false},
        {"rnbqkb1r/ppp1pppp/5n2/8/3PP3/2N5/PP3PPP/R1BQKBNR b KQkq - 3 5", create_move(SQ_E7, SQ_E5), true, false, false},
        /* {"rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ - 1 5", create_move(SQ_D2, SQ_C3), true, false, false}, */
        {"r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 12", create_move(SQ_F8, SQ_B8), true, false, false},
        {"rn1qkb1r/pb1p1ppp/1p2pn2/2p5/2PP4/5NP1/PP2PPBP/RNBQK2R w KQkq c6 1 6", create_move(SQ_D4, SQ_D5), true, false, false},
        {"r1bq1rk1/1pp2pbp/p1np1np1/3Pp3/2P1P3/2N1BP2/PP4PP/R1NQKB1R b KQ - 1 9", create_move(SQ_C6, SQ_D4), true, false, false},
        {"rnbqr1k1/1p3pbp/p2p1np1/2pP4/4P3/2N5/PP1NBPPP/R1BQ1RK1 w - - 1 11", create_move(SQ_A2, SQ_A4), true, false, false},
        {"rnbqkb1r/pppp1ppp/5n2/4p3/4PP2/2N5/PPPP2PP/R1BQKBNR b KQkq f3 1 3", create_move(SQ_D7, SQ_D5), true, false, false},
        {"r1bqk1nr/pppnbppp/3p4/8/2BNP3/8/PPP2PPP/RNBQK2R w KQkq - 2 6", create_move(SQ_C4, SQ_F7), false, true, true},
        {"rnbq1b1r/ppp2kpp/3p1n2/8/3PP3/8/PPP2PPP/RNBQKB1R b KQ d3 1 5", create_move(SQ_F6, SQ_E4), false, true, false},
        {"rnbqkb1r/pppp1ppp/3n4/8/2BQ4/5N2/PPP2PPP/RNB2RK1 b kq - 1 6", create_move(SQ_D6, SQ_C4), false, true, false},
        {"r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 w - - 1 12", create_move(SQ_E5, SQ_F6), false, true, false},
        {"r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - 2 7", create_move(SQ_D7, SQ_D5), true, false, false},
        {"r2qkbnr/2p2pp1/p1pp4/4p2p/4P1b1/5N1P/PPPP1PP1/RNBQ1RK1 w kq - 1 8", create_move(SQ_D2, SQ_D4), true, false, false},
        /* {"1rbq1rk1/p1b1nppp/1p2p3/8/1B1pN3/P2B4/1P3PPP/2RQ1R1K w - - 0 1", create_move(SQ_G4, SQ_F6), true, false, false}, */
        {"3r2k1/p2r1p1p/1p2p1p1/q4n2/3P4/PQ5P/1P1RNPP1/3R2K1 b - - 0 1", create_move(SQ_F5, SQ_D4), false, true, false},
        {"r1b1r1k1/1ppn1p1p/3pnqp1/8/p1P1P3/5P2/PbNQNBPP/1R2RB1K w - - 0 1", create_move(SQ_B1, SQ_B2), false, true, false},
        {"2r4k/pB4bp/1p4p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - - 0 1", create_move(SQ_G5, SQ_C1), false, true, false},
        {"5r1k/6pp/1n2Q3/4p3/8/7P/PP4PK/R1B1q3 b - - 0 1", create_move(SQ_H7, SQ_H6), true, false, false},
        {"r3k2r/pbn2ppp/8/1P1pP3/P1qP4/5B2/3Q1PPP/R3K2R w KQkq - 0 1", create_move(SQ_F3, SQ_E2), true, false, false},
        {"3r2k1/ppq2pp1/4p2p/3n3P/3N2P1/2P5/PP2QP2/K2R4 b - - 0 1", create_move(SQ_D5, SQ_C3), false, true, false},
        {"q3rn1k/2QR4/pp2pp2/8/P1P5/1P4N1/6n1/6K1 w - - 0 1", create_move(SQ_G3, SQ_F5), true, false, false},
        {"6k1/p3q2p/1nr3pB/8/3Q1P2/6P1/PP5P/3R2K1 b - - 0 1", create_move(SQ_B6, SQ_D6), true, false, false},
        {"1r4k1/7p/5np1/3p3n/8/2NB4/7P/3N1RK1 w - - 0 1", create_move(SQ_C3, SQ_D5), false, true, false},
        {"1r2r1k1/p4p1p/6pB/q7/8/3Q2P1/PbP2PKP/1R3R2 w - - 0 1", create_move(SQ_B1, SQ_B2), false, true, false},
        {"r2q1r1k/pb3p1p/2n1p2Q/5p2/8/3B2N1/PP3PPP/R3R1K1 w - - 0 1", create_move(SQ_D3, SQ_F5), false, true, false},
        {"8/4p3/p2p4/2pP4/2P1P3/1P4k1/1P1K4/8 w - - 0 1", create_move(SQ_B2, SQ_B4), true, false, false},
        {"1r1q1rk1/p1p2pbp/2pp1np1/6B1/4P3/2NQ4/PPP2PPP/3R1RK1 w - - 0 1", create_move(SQ_E4, SQ_E5), true, false, false},
        {"q4rk1/1n1Qbppp/2p5/1p2p3/1P2P3/2P4P/6P1/2B1NRK1 b - - 0 1", create_move(SQ_A8, SQ_C8), true, false, false},
        {"r2q1r1k/1b1nN2p/pp3pp1/8/Q7/PP5P/1BP2RPN/7K w - - 0 1", create_move(SQ_A4, SQ_D7), false, true, false},
        {"8/5p2/pk2p3/4P2p/2b1pP1P/P3P2B/8/7K w - - 0 1", create_move(SQ_H3, SQ_G4), true, false, false},
        {"8/2k5/4p3/1nb2p2/2K5/8/6B1/8 w - - 0 1", create_move(SQ_C4, SQ_B5), false, true, false},
        {"1B1b4/7K/1p6/1k6/8/8/8/8 w - - 0 1", create_move(SQ_B8, SQ_A7), true, false, false},
        {"rn1q1rk1/1b2bppp/1pn1p3/p2pP3/3P4/P2BBN1P/1P1N1PP1/R2Q1RK1 b - - 0 1", create_move(SQ_B7, SQ_A6), true, false, false},
        {"8/p1ppk1p1/2n2p2/8/4B3/2P1KPP1/1P5P/8 w - - 0 1", create_move(SQ_E4, SQ_C6), false, true, false},
        {"8/3nk3/3pp3/1B6/8/3PPP2/4K3/8 w - - 0 1", create_move(SQ_B5, SQ_D7), false, true, false},
        {"rnbq1k1r/ppp1n1pp/3p4/1Bb1p3/8/4P2N/PPPP2PP/RNBQK2R w KQ - 0 1", KING_CASTLING_MOVE, true, false, true},
        {"4r3/ppp5/2np4/8/2B3b1/4PN2/5PPP/1k2K2R w K - 0 1", KING_CASTLING_MOVE, true, false, true},
        {"1n1k4/ppp2ppp/6q1/1B2p3/1b2P1b1/2N2N2/PPP2PPP/R3K1R1 w Q - 0 1", QUEEN_CASTLING_MOVE, true, false, true},
        {"7b/7b/8/R1pP3k/4P3/8/K7/8 w - c6 0 2", create_move(SQ_D5, SQ_C6), false, true, true},
    };

    for (const TestCase& test_case : test_cases)
    {
        Position position(std::get<0>(test_case));
        Move move = std::get<1>(test_case);

        EXPECT_EQ(position.move_is_quiet(move), std::get<2>(test_case)) << std::get<0>(test_case);
        EXPECT_EQ(position.move_is_capture(move), std::get<3>(test_case)) << std::get<0>(test_case);
        EXPECT_EQ(position.move_gives_check(move), std::get<4>(test_case)) << std::get<0>(test_case);
    }
}

/* TEST(ScoreTest, knight) */
/* { */
/*     Position position("rnbqkb1r/pp2pppp/2p5/3pP3/4n3/2N2N2/PPPP1PPP/R1BQKB1R w - -"); */
/*     PositionScorer scorer; */
/*     std::cout << scorer.score(position) << std::endl; */
/* } */

}
