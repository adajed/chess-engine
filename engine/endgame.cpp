#include "endgame.h"

#include "bithacks.h"
#include "types.h"
#include "value.h"

#include <cassert>
#include <cstdlib>
#include <memory>
#include <vector>

namespace engine
{
namespace endgame
{

enum EndgameType : uint32_t
{
    kKPK,
    kKPsK,
    kKNBK,
    kKXK,
    kKQKR,
    kKRNKR,
    kKRBKR,
    kKBPsK,
    kKQKP,
    kKRKP,
    kKNNK,
    kKNNKP,
    kKBPsKB,
    kKRKB,
    kKRKN,
    kKQKRPs,
    kKmmKm,
};

namespace
{
// clang-format off
/**
 * Weights to push weak king to edges and corners.
 */
int PUSH_TO_EDGE_BONUS[SQUARE_NUM] = {
    100, 90, 80, 70, 70, 80, 90, 100,
     90, 60, 50, 40, 40, 50, 60,  90,
     80, 50, 30, 20, 20, 30, 40,  80,
     70, 40, 20, 10, 10, 20, 40,  70,
     70, 40, 20, 10, 10, 20, 40,  70,
     80, 50, 30, 20, 20, 30, 40,  80,
     90, 60, 50, 40, 40, 50, 60,  90,
    100, 90, 80, 70, 70, 80, 90, 100,
};

/**
 * Weights to push weak king to corner of correct color.
 * Default is to push to black corners, for white
 * board needs to be flipped horizentaly.
 */
int PUSH_TO_COLOR_CORNER_BONUS[SQUARE_NUM] = {
    100, 90, 80, 70, 70, 60, 50,  40,
     90, 60, 50, 40, 40, 50, 60,  50,
     80, 50, 30, 20, 20, 30, 40,  60,
     70, 40, 20, 10, 10, 20, 40,  70,
     70, 40, 20, 10, 10, 20, 40,  70,
     60, 50, 30, 20, 20, 30, 40,  80,
     50, 60, 50, 40, 40, 50, 60,  90,
     40, 50, 60, 70, 70, 80, 90, 100,
};
// clang-format on


/**
 * Weights to have both kings close to each other.
 */
int PUSH_CLOSE[RANK_NUM] = {0, 7, 6, 5, 4, 3, 2, 1};

Square most_advanced_pawn(Bitboard pawns, Color side)
{
    return Square(side == WHITE ? msb(pawns) : lsb(pawns));
}

template <EndgameType endgameType>
struct SandboxPCV
{
};

template <>
struct SandboxPCV<kKPK> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(1, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        create_pcv(0, 0, 0, 0, 0, 1, 0, 0, 0, 0)
    };
};

template <>
struct SandboxPCV<kKNBK> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 1, 1, 0, 0, 0, 0, 0, 0, 0),
        create_pcv(0, 0, 0, 0, 0, 0, 1, 1, 0, 0)
    };
};

template <>
struct SandboxPCV<kKQKR> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 0, 0, 0, 1, 0, 0, 0, 1, 0),
        create_pcv(0, 0, 0, 1, 0, 0, 0, 0, 0, 1)
    };
};

template <>
struct SandboxPCV<kKRNKR> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 1, 0, 1, 0, 0, 0, 0, 1, 0),
        create_pcv(0, 0, 0, 1, 0, 0, 1, 0, 1, 0)
    };
};

template <>
struct SandboxPCV<kKRBKR> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 0, 1, 1, 0, 0, 0, 0, 1, 0),
        create_pcv(0, 0, 0, 1, 0, 0, 0, 1, 1, 0)
    };
};

template <>
struct SandboxPCV<kKQKP> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 0, 0, 0, 1, 1, 0, 0, 0, 0),
        create_pcv(1, 0, 0, 0, 0, 0, 0, 0, 0, 1)
    };
};

template <>
struct SandboxPCV<kKRKP> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 0, 0, 1, 0, 1, 0, 0, 0, 0),
        create_pcv(1, 0, 0, 0, 0, 0, 0, 0, 1, 0)
    };
};

template <>
struct SandboxPCV<kKNNK>
{
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 2, 0, 0, 0, 0, 0, 0, 0, 0),
        create_pcv(0, 0, 0, 0, 0, 0, 2, 0, 0, 0)
    };
};

template <>
struct SandboxPCV<kKNNKP>
{
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 2, 0, 0, 0, 1, 0, 0, 0, 0),
        create_pcv(1, 0, 0, 0, 0, 0, 2, 0, 0, 0)
    };
};

template <>
struct SandboxPCV<kKRKB> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 0, 0, 1, 0, 0, 0, 1, 0, 0),
        create_pcv(0, 0, 1, 0, 0, 0, 0, 0, 1, 0)
    };
};

template <>
struct SandboxPCV<kKRKN> {
    static constexpr PieceCountVector pcv[COLOR_NUM] = {
        create_pcv(0, 0, 0, 1, 0, 0, 1, 0, 0, 0),
        create_pcv(0, 1, 0, 0, 0, 0, 0, 0, 1, 0)
    };
};


// clang-format on
template <EndgameType endgameType>
class Endgame : public EndgameBase
{
  public:
    Endgame(Color strong) : EndgameBase(strong) {}

    bool applies(const Position& position) const override
    {
        return position.get_pcv() == SandboxPCV<endgameType>::pcv[strongSide];
    }

    virtual ~Endgame() = default;

  protected:
    Value strongSideScore(const Position& position) const override;
};

template <>
Value Endgame<kKPK>::strongSideScore(const Position& position) const
{
    assert(applies(position));

    Color side = position.color();
    Square strongKingSq = position.piece_position(strongKing, 0);
    Square weakKingSq = position.piece_position(weakKing, 0);
    Square strongPawn =
        position.piece_position(make_piece(strongSide, PAWN), 0);

    bitbase::normalize(strongSide, side, strongKingSq, strongPawn, weakKingSq);
    if (!bitbase::check(side, strongKingSq, strongPawn, weakKingSq))
        return VALUE_POSITIVE_DRAW + Value(rank(normalize(strongPawn, strongSide)));

    return VALUE_KNOWN_WIN + Value(rank(normalize(strongPawn, strongSide)));
}

template <>
bool Endgame<kKPsK>::applies(const Position& position) const
{
    return position.no_nonpawns(strongSide) == 0
        && position.no_pieces(make_piece(strongSide, PAWN)) >= 2
        && position.pieces(weakSide) == position.pieces(weakSide, KING);
}

template <>
Value Endgame<kKPsK>::strongSideScore(const Position& position) const
{
    const Bitboard pawns = position.pieces(strongSide, PAWN);
    const File pawnFile = file(Square(lsb(pawns)));
    const Square queeningSq = normalize(make_square(RANK_8, pawnFile), strongSide);
    const Square weakKingSq = position.piece_position(make_piece(weakSide, KING));

    // It's a draw if:
    // 1. All pawns are on the same rook file (A or H).
    // 2. Weak king can reach queening square.
    if ((pawns == (pawns & fileA_bb) || pawns == (pawns & fileH_bb)) &&
            distance(weakKingSq, queeningSq) <= 1)
    {
        return VALUE_POSITIVE_DRAW;
    }

    return VALUE_KNOWN_WIN
         + PIECE_VALUE[PAWN].eg * position.no_pieces(make_piece(strongSide, PAWN))
         + Value(rank(normalize(most_advanced_pawn(pawns, strongSide), strongSide)));
}

template <>
Value Endgame<kKNBK>::strongSideScore(const Position& position) const
{
    Square weakKingSq = position.piece_position(weakKing, 0);
    Square bishop = position.piece_position(make_piece(strongSide, BISHOP), 0);
    Color bishopColor = (static_cast<int>(rank(bishop)) + static_cast<int>(file(bishop))) & 1 ? WHITE : BLACK;

    Square kingSquare =
        bishopColor == WHITE ? flip_vertically(weakKingSq) : weakKingSq;
    Value v = Value(PUSH_TO_COLOR_CORNER_BONUS[kingSquare]);

    return Value(std::min(int64_t(VALUE_KNOWN_WIN + v), int64_t(VALUE_MATE - 1)));
}

template <>
Value Endgame<kKQKR>::strongSideScore(const Position& position) const
{
    Square strongKingSq = position.piece_position(strongKing, 0);
    Square weakKingSq = position.piece_position(weakKing, 0);

    Value v = PIECE_VALUE[QUEEN].eg - PIECE_VALUE[ROOK].eg
            + PUSH_TO_EDGE_BONUS[weakKingSq]
            + PUSH_CLOSE[distance(strongKingSq, weakKingSq)];

    return Value(std::min(int64_t(VALUE_KNOWN_WIN + v), int64_t(VALUE_MATE - 1)));
}

template <>
bool Endgame<kKXK>::applies(const Position& position) const
{
    return popcount(position.pieces(weakSide)) == 1;
}

template <>
Value Endgame<kKXK>::strongSideScore(const Position& position) const
{
    const Square strongKingSq = position.piece_position(strongKing, 0);
    const Square weakKingSq = position.piece_position(weakKing, 0);

    Value v = VALUE_DRAW;
    v += PIECE_VALUE[PAWN].eg * position.no_pieces(make_piece(strongSide, PAWN));
    v += PIECE_VALUE[KNIGHT].eg * position.no_pieces(make_piece(strongSide, KNIGHT));
    v += PIECE_VALUE[BISHOP].eg * position.no_pieces(make_piece(strongSide, BISHOP));
    v += PIECE_VALUE[ROOK].eg * position.no_pieces(make_piece(strongSide, ROOK));
    v += PIECE_VALUE[QUEEN].eg * position.no_pieces(make_piece(strongSide, QUEEN));
    v += PUSH_TO_EDGE_BONUS[weakKingSq] +
         PUSH_CLOSE[distance(strongKingSq, weakKingSq)];

    return Value(std::min(int64_t(v + VALUE_KNOWN_WIN), int64_t(VALUE_MATE - 1)));
}

template <>
Value Endgame<kKRNKR>::strongSideScore(const Position& position) const
{
    const Square weakKingSq = position.piece_position(weakKing, 0);
    return VALUE_POSITIVE_DRAW + PUSH_TO_EDGE_BONUS[weakKingSq];
}

template <>
Value Endgame<kKRBKR>::strongSideScore(const Position& position) const
{
    const Square weakKingSq = position.piece_position(weakKing, 0);
    return VALUE_POSITIVE_DRAW + PUSH_TO_EDGE_BONUS[weakKingSq];
}

template <>
bool Endgame<kKRBKR>::applies(const Position& position) const
{
    return position.get_pcv() == SandboxPCV<kKRBKR>::pcv[strongSide];
}

template <>
bool Endgame<kKBPsK>::applies(const Position& position) const
{
    return position.no_pieces(make_piece(strongSide, BISHOP)) == 1
        && position.no_nonpawns(strongSide) == 1
        && position.no_pieces(make_piece(strongSide, PAWN)) > 0
        && position.pieces(weakSide) == position.pieces(weakSide, KING);
}

template <>
Value Endgame<kKBPsK>::strongSideScore(const Position& position) const
{
    const Bitboard pawns = position.pieces(strongSide, PAWN);
    const File pawnFile = file(Square(lsb(pawns)));
    const Square queeningSq = normalize(make_square(RANK_8, pawnFile), strongSide);
    const Square bishopSq = position.piece_position(make_piece(strongSide, BISHOP));
    const Square weakKingSq = position.piece_position(make_piece(weakSide, KING));

    // It's a draw if:
    // 1. All pawns are on the same rook file (A or H).
    // 2. Queening square is different color than the bishop.
    // 3. Weak king can reach queening square.
    if ((pawns == (pawns & fileA_bb) || pawns == (pawns & fileH_bb)) &&
            sq_color(queeningSq) != sq_color(bishopSq) &&
            distance(weakKingSq, queeningSq) <= 1)
    {
        return VALUE_POSITIVE_DRAW;
    }

    return VALUE_KNOWN_WIN
        + PIECE_VALUE[PAWN].eg * position.no_pieces(make_piece(strongSide, PAWN))
        + PIECE_VALUE[BISHOP].eg
        + Value(rank(normalize(most_advanced_pawn(pawns, strongSide), strongSide)));
}

template <>
Value Endgame<kKQKP>::strongSideScore(const Position& position) const
{
    const Square strongKingSq = normalize(position.piece_position(make_piece(strongSide, KING)), strongSide);
    const Square weakKingSq = normalize(position.piece_position(make_piece(weakSide, KING)), strongSide);
    // const Square queenSq = normalize(position.piece_position(make_piece(strongSide, QUEEN)), strongSide);
    const Square pawnSq = normalize(position.piece_position(make_piece(weakSide, PAWN)), strongSide);
    const Square queeningSq = make_square(RANK_1, file(pawnSq));

    // if pawn is A/C/F/H-pawn at 2nd rank and weakKing controls queening square then it's a draw
    if (rank(pawnSq) == RANK_2 &&
        (position.pieces(weakSide, PAWN) & (fileA_bb | fileC_bb | fileF_bb | fileH_bb)) &&
        distance(weakKingSq, queeningSq) <= 1)
    {
        return VALUE_POSITIVE_DRAW + PUSH_CLOSE[distance(strongKingSq, pawnSq)];
    }

    return VALUE_KNOWN_WIN + PUSH_CLOSE[distance(strongKingSq, pawnSq)];
}

template <>
Value Endgame<kKRKP>::strongSideScore(const Position& position) const
{
    const Square strongKingSq = normalize(position.piece_position(make_piece(strongSide, KING)), strongSide);
    const Square weakKingSq = normalize(position.piece_position(make_piece(weakSide, KING)), strongSide);
    /* const Square rookSq = normalize(position.piece_position(make_piece(strongSide, ROOK)), strongSide); */
    const Square pawnSq = normalize(position.piece_position(make_piece(weakSide, PAWN)), strongSide);
    const Square queeningSq = make_square(RANK_1, file(pawnSq));

    // if strongKing is on front of the pawn
    if (rank(strongKingSq) < rank(pawnSq) && std::abs(file(strongKingSq) - file(pawnSq)) <= 1)
        return VALUE_KNOWN_WIN + PUSH_CLOSE[distance(strongKingSq, pawnSq)];

    // if pawn is far advanced and supported by king and strongKing is far away
    if (rank(pawnSq) < RANK_5 &&
        distance(weakKingSq, pawnSq) <= 1 &&
        distance(strongKingSq, pawnSq) > 2)
        return VALUE_POSITIVE_DRAW + Value(rank(pawnSq));

    return PIECE_VALUE[ROOK].eg - PIECE_VALUE[PAWN].eg - PUSH_CLOSE[distance(pawnSq, queeningSq)];
}

template <>
Value Endgame<kKNNK>::strongSideScore(const Position& /* position */) const
{
    return VALUE_DRAW;
}

template <>
Value Endgame<kKNNKP>::strongSideScore(const Position& position) const
{
    const Square strongKingSq = normalize(position.piece_position(make_piece(strongSide, KING)), strongSide);
    const Square weakKingSq = normalize(position.piece_position(make_piece(weakSide, KING)), strongSide);
    const Square pawnSq = normalize(position.piece_position(make_piece(weakSide, PAWN)), strongSide);
    const Square knight1Sq = normalize(position.piece_position(make_piece(strongSide, KNIGHT), 0), strongSide);
    const Square knight2Sq = normalize(position.piece_position(make_piece(strongSide, KNIGHT), 1), strongSide);

    // Try to push weakKing to the corner and not allow pawn to be pushed too far.
    // Keep strong pieces (king and knights) close to weakKing.
    return PIECE_VALUE[PAWN].eg
         + 5 * PUSH_CLOSE[distance(strongKingSq, weakKingSq)]
         + 5 * PUSH_CLOSE[distance(knight1Sq, weakKingSq)]
         + 5 * PUSH_CLOSE[distance(knight2Sq, weakKingSq)]
         + PUSH_TO_EDGE_BONUS[weakKingSq]
         + 30 * Value(rank(pawnSq));
}

template <>
bool Endgame<kKBPsKB>::applies(const Position& position) const
{
    return position.no_pieces(make_piece(strongSide, BISHOP)) == 1
        && position.no_pieces(make_piece(weakSide, BISHOP)) == 1
        && position.no_pieces(make_piece(strongSide, PAWN)) >= 1
        && position.no_pieces(make_piece(weakSide, PAWN)) == 0
        && position.no_nonpawns(strongSide) == 1
        && position.no_nonpawns(weakSide) == 1;
}

template <>
Value Endgame<kKBPsKB>::strongSideScore(const Position& position) const
{
    const Bitboard pawns = position.pieces(strongSide, PAWN);
    const Square furthestPawnSq = normalize(most_advanced_pawn(pawns, strongSide), strongSide);
    const Square weakKingSq = normalize(position.piece_position(make_piece(weakSide, KING)), strongSide);
    const Square strongBishopSq = normalize(position.piece_position(make_piece(strongSide, BISHOP)), strongSide);
    const Square weakBishopSq = normalize(position.piece_position(make_piece(weakSide, BISHOP)), strongSide);

    /*
     * In draw cases try to not lose pawns and push them if possible
     */
    if (all_on_same_file(pawns))
    {
        if (file(weakKingSq) == file(furthestPawnSq) &&
                rank(weakKingSq) > rank(furthestPawnSq) && 
                sq_color(weakKingSq) != sq_color(strongBishopSq))
            return VALUE_POSITIVE_DRAW + 10 * Value(popcount(pawns)) + 2 * Value(rank(furthestPawnSq));
    }
    else if (sq_color(strongBishopSq) != sq_color(weakKingSq))
    {
        bool pawnOnFile[FILE_NUM] = {false};
        File file1 = file(furthestPawnSq);
        File file2 = file1;
        for (int i = 0; i < position.no_pieces(make_piece(strongSide, PAWN)); ++i)
        {
            File f = file(position.piece_position(make_piece(strongSide, PAWN), i));
            if (f != file1) file2 = f;
            pawnOnFile[f] = true;
        }

        int fileCount = 0;
        for (int i = 0; i < static_cast<int>(FILE_NUM); ++i)
            fileCount += static_cast<int>(pawnOnFile[i]);

        if (fileCount == 2)
        {
            const Square furthestPawn2Sq = normalize(most_advanced_pawn(pawns & FILES_BB[file2], strongSide), strongSide);
            if (std::abs(file1 - file2) == 1 &&
                    !popcount_more_than_one(pawns & FILES_BB[file1]) &&
                    rank(furthestPawnSq) > rank(furthestPawn2Sq) &&
                    sq_color(furthestPawnSq) == sq_color(strongBishopSq))
            {
                const Square block1Sq = make_square(Rank(rank(furthestPawnSq) + 1), file1);
                const Square block2Sq = make_square(rank(furthestPawnSq), file2);
                if (weakKingSq ==  block1Sq &&
                        (weakBishopSq == block2Sq ||
                         slider_attack<BISHOP>(weakBishopSq, position.pieces()) & square_bb(block2Sq)))
                    return VALUE_POSITIVE_DRAW + 10 * Value(popcount(pawns)) + 2 * Value(rank(furthestPawnSq));
                if (weakKingSq == block2Sq &&
                        (weakBishopSq == block1Sq ||
                         slider_attack<BISHOP>(weakBishopSq, position.pieces()) & square_bb(block1Sq)))
                    return VALUE_POSITIVE_DRAW + 10 * Value(popcount(pawns)) + 2 * Value(rank(furthestPawnSq));
            }
        }
    }

    return popcount(pawns) * PIECE_VALUE[PAWN].eg + 10 * Value(rank(furthestPawnSq));
}

template <>
Value Endgame<kKRKB>::strongSideScore(const Position& position) const
{
    const Square weakKingSq = normalize(position.piece_position(make_piece(weakSide, KING)), strongSide);
    return VALUE_POSITIVE_DRAW + PUSH_TO_EDGE_BONUS[weakKingSq];
}

template <>
Value Endgame<kKRKN>::strongSideScore(const Position& position) const
{
    const Square weakKingSq = normalize(position.piece_position(make_piece(weakSide, KING)), strongSide);
    const Square knightSq = normalize(position.piece_position(make_piece(weakSide, KNIGHT)), strongSide);
    return VALUE_POSITIVE_DRAW + PUSH_TO_EDGE_BONUS[weakKingSq] + 10 * distance(weakKingSq, knightSq);
}

template <>
bool Endgame<kKQKRPs>::applies(const Position& position) const
{
    return position.no_pieces(make_piece(strongSide, QUEEN)) == 1
        && position.no_pieces(make_piece(weakSide, ROOK)) == 1
        && position.no_pieces(make_piece(strongSide, PAWN)) == 0
        && position.no_pieces(make_piece(weakSide, PAWN)) >= 1
        && position.no_nonpawns(strongSide) == 1
        && position.no_nonpawns(weakSide) == 1;
}

template <>
Value Endgame<kKQKRPs>::strongSideScore(const Position& position) const
{
    const Bitboard pawns = position.pieces(weakSide, PAWN);
    const Square weakKingSq = position.piece_position(make_piece(weakSide, KING));
    /* const Square strongKingSq = position.piece_position(make_piece(strongSide, KING)); */
    const Square rookSq = position.piece_position(make_piece(weakSide, ROOK));

    if ((pawn_attacks(pawns, weakSide) & square_bb(rookSq)) && // pawns defend the rook
        (KING_MASK[weakKingSq] & pawns)) // king defends some pawn
        return VALUE_POSITIVE_DRAW + 10 * Value(rank(normalize(most_advanced_pawn(pawns, weakSide), strongSide)));

    return PIECE_VALUE[QUEEN].eg - PIECE_VALUE[ROOK].eg
         - popcount(pawns) * PIECE_VALUE[PAWN].eg;
}

template <>
bool Endgame<kKmmKm>::applies(const Position& position) const
{
    const int strongMinorsCount = position.no_pieces(make_piece(strongSide, KNIGHT))
                                + position.no_pieces(make_piece(strongSide, BISHOP));
    const int weakMinorsCount = position.no_pieces(make_piece(weakSide, KNIGHT))
                              + position.no_pieces(make_piece(weakSide, BISHOP));
    return strongMinorsCount == 2 && weakMinorsCount == 1
        && position.no_pieces(make_piece(strongSide, PAWN)) == 0
        && position.no_pieces(make_piece(weakSide, PAWN)) == 0
        && position.no_nonpawns(strongSide) == 2
        && position.no_nonpawns(weakSide) == 1;
}

template <>
Value Endgame<kKmmKm>::strongSideScore(const Position& position) const
{
    if (!(position.no_pieces(make_piece(strongSide, BISHOP)) == 2 &&
            position.no_pieces(make_piece(weakSide, KNIGHT)) == 1))
        return VALUE_POSITIVE_DRAW;
    const Square bishop1Sq = position.piece_position(make_piece(strongSide, BISHOP), 0);
    const Square bishop2Sq = position.piece_position(make_piece(strongSide, BISHOP), 1);
    return sq_color(bishop1Sq) != sq_color(bishop2Sq)
         ? 2 * PIECE_VALUE[BISHOP].eg - PIECE_VALUE[KNIGHT].eg
         : VALUE_POSITIVE_DRAW;
}


}  // namespace

using EndgameBasePtr = std::unique_ptr<EndgameBase>;

std::vector<EndgameBasePtr> endgames;

template <EndgameType endgameType>
void add()
{
    endgames.push_back(
        std::unique_ptr<EndgameBase>(new Endgame<endgameType>(WHITE)));
    endgames.push_back(
        std::unique_ptr<EndgameBase>(new Endgame<endgameType>(BLACK)));
}

void init()
{
    add<kKPK>();
    add<kKPsK>();
    add<kKRKB>();
    add<kKRKN>();
    add<kKNNK>();
    add<kKNNKP>();
    add<kKQKR>();
    add<kKNBK>();
    add<kKRNKR>();
    add<kKRBKR>();
    add<kKBPsK>();
    add<kKBPsKB>();
    add<kKRKP>();
    add<kKQKP>();
    add<kKQKRPs>();
    add<kKmmKm>();

    // kKXK must be last
    add<kKXK>();
}

Value score(const Position& position)
{
    for (const EndgameBasePtr& e : endgames)
    {
        if (e->applies(position)) return e->score(position);
    }

    return VALUE_NONE;
}

}  // namespace endgame
}  // namespace engine
