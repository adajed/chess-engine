#include "endgame.h"
#include "types.h"
#include <cassert>
#include <memory>
#include <vector>

namespace engine
{
namespace endgame
{

namespace
{
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

/**
 * Weights to have both kings close to each other.
 */
int PUSH_CLOSE[RANK_NUM] = {
    0, 7, 6, 5, 4, 3, 2, 1
};

}  // namespace anonymous

template<>
bool Endgame<kKPK>::applies(const Position& position) const
{
    if (position.number_of_pieces(make_piece(strongSide, PAWN)) != 1
            || position.number_of_pieces(make_piece(weakSide, PAWN)) != 0
            || position.pieces(KNIGHT)
            || position.pieces(BISHOP)
            || position.pieces(ROOK)
            || position.pieces(QUEEN))
        return false;

    return true;
}

template<>
Value Endgame<kKPK>::score(const Position& position) const
{
    assert(applies(position));

    Color side = position.side_to_move();
    Square strongKing = position.piece_position(make_piece(strongSide, KING), 0);
    Square strongPawn = position.piece_position(make_piece(strongSide, PAWN), 0);
    Square weakKing   = position.piece_position(make_piece(weakSide, KING), 0);

    bitbase::normalize(strongSide, side, strongKing, strongPawn, weakKing);
    if (!bitbase::check(side, strongKing, strongPawn, weakKing))
        return VALUE_DRAW;

    Value v = VALUE_KNOWN_WIN + Value(rank(strongPawn));
    return (position.side_to_move() == strongSide) ? v : (-v);
}

template<>
bool Endgame<kKNBK>::applies(const Position& position) const
{
    if (position.number_of_pieces(make_piece(strongSide, KNIGHT)) != 1
            || position.number_of_pieces(make_piece(strongSide, BISHOP)) != 1
            || position.number_of_pieces(make_piece(weakSide, KNIGHT)) != 0
            || position.number_of_pieces(make_piece(weakSide, BISHOP)) != 0
            || position.pieces(PAWN)
            || position.pieces(ROOK)
            || position.pieces(QUEEN))
        return false;

    return true;
}

template<>
Value Endgame<kKNBK>::score(const Position& position) const
{
    Square strongKing = position.piece_position(make_piece(strongSide, KING), 0);
    Square weakKing   = position.piece_position(make_piece(weakSide, KING), 0);
    Square bishop = position.piece_position(make_piece(strongSide, BISHOP), 0);
    Color bishopColor = (rank(bishop) + file(bishop)) & 1 ? WHITE : BLACK;

    Square kingSquare = bishopColor == WHITE ? flip_vertically(weakKing) : weakKing;
    Value v = Value(PUSH_TO_COLOR_CORNER_BONUS[kingSquare]);

    v = Value(std::min(int64_t(VALUE_KNOWN_WIN + v), int64_t(VALUE_MATE - 1)));
    return (position.side_to_move() == strongSide) ? v : (-v);
}

template <>
bool Endgame<kKXK>::applies(const Position& position) const
{
    return true;
}

template<>
Value Endgame<kKXK>::score(const Position& position) const
{
    Square strongKing = position.piece_position(make_piece(strongSide, KING), 0);
    Square weakKing   = position.piece_position(make_piece(weakSide, KING), 0);

    Value v = VALUE_DRAW;
    v += 100 * position.number_of_pieces(make_piece(strongSide, PAWN));
    v += 300 * position.number_of_pieces(make_piece(strongSide, KNIGHT));
    v += 300 * position.number_of_pieces(make_piece(strongSide, BISHOP));
    v += 500 * position.number_of_pieces(make_piece(strongSide, ROOK));
    v += 900 * position.number_of_pieces(make_piece(strongSide, QUEEN));
    v += PUSH_TO_EDGE_BONUS[weakKing] + PUSH_CLOSE[distance(strongKing, weakKing)];

    v = Value(std::min(int64_t(v + VALUE_KNOWN_WIN), int64_t(VALUE_MATE - 1)));
    return (position.side_to_move() == strongSide) ? v : (-v);
}

std::vector<EndgamePair> endgames;
EndgamePair default_endgame = std::make_pair(std::make_unique<Endgame<kKXK>>(WHITE),
                                             std::make_unique<Endgame<kKXK>>(BLACK));

template <EndgameType endgameType>
void add()
{
    endgames.push_back(std::make_pair(std::unique_ptr<EndgameBase>(new Endgame<endgameType>(WHITE)),
                                      std::unique_ptr<EndgameBase>(new Endgame<endgameType>(BLACK))));
}

void init()
{
    add<kKPK>();
    add<kKNBK>();
}

}  // namespace endgame
}  // namespace engine
