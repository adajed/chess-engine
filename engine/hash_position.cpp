#include "hash_position.h"

namespace engine
{

uint64_t hash_position(Position const& position)
{
    uint64_t key = 0ULL;

    for (Piece piece = W_PAWN; piece <= B_KING; ++piece)
    {
        int size = position.no_pieces(piece);
        for (int i = 0; i < size; ++i)
            key ^= HASH_PIECE[piece][position.piece_position(piece, i)];
    }

    if (position.castling_rights() & W_OO) key ^= HASH_CASTLING_WHITE_SHORT;
    if (position.castling_rights() & W_OOO) key ^= HASH_CASTLING_WHITE_LONG;
    if (position.castling_rights() & B_OO) key ^= HASH_CASTLING_BLACK_SHORT;
    if (position.castling_rights() & B_OOO) key ^= HASH_CASTLING_BLACK_LONG;

    if (position.enpassant_square() != NO_SQUARE)
    {
        Bitboard enpassant_bb = square_bb(position.enpassant_square());
        Bitboard possible_attackers = pawn_attacks(enpassant_bb, !position.color());

        if (possible_attackers & position.pieces(position.color(), PAWN))
            key ^= HASH_ENPASSANT[file(position.enpassant_square())];
    }

    if (position.color() == WHITE) key ^= HASH_TURN;

    return key;
}


}  // namespace engine
