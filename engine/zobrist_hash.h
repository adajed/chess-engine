#ifndef CHESS_ENGINE_HASH_H_
#define CHESS_ENGINE_HASH_H_

#include "types.h"

namespace engine
{
class Position;

class HashKey
{
  public:
    HashKey();

    HashKey(PieceCountVector pcv);

    HashKey(const std::string& syzygyCode, Color c);

    void init(const Position& position);

    uint64_t get_key() const;

    uint64_t get_pawnkey() const;

    void move_piece(Piece piece, Square from, Square to);

    void toggle_piece(Piece piece, Square sq);

    void flip_side();

    void clear_enpassant();

    void set_enpassant(File file);

    void clear_castling();

    void set_castling(Castling castling);

  private:
    uint64_t _piece_key;
    uint64_t _pawn_key;
    uint64_t _enpassant_key;
    uint64_t _castling_key;
    uint64_t _color_key;
};

}  // namespace engine

#endif  // CHESS_ENGINE_HASH_H_
