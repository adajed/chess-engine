#ifndef CHESS_ENGINE_SCORE_H_
#define CHESS_ENGINE_SCORE_H_

#include "hashmap.h"
#include "movegen.h"
#include "types.h"
#include "value.h"

namespace engine
{
using PawnHashMap = HashMap<uint64_t, Score, 512 * 512>;

class PositionScorer
{
  public:
    PositionScorer();

    Value score(const Position& position);

    void print_stats();

    void clear();

  private:
    template <Color side>
    void setup(const Position& position);

    Score score_pieces(const Position& position);

    template <Color side>
    Score score_pieces_for_side(const Position& position);

    Score score_pawns(const Position& position);

    template <Color side>
    Score score_pawns_for_side(const Position& position);

    template <Color side>
    Score score_king(const Position& position);

    template <Color side>
    Score score_king_shelter(const Position& position, Square king_sq, int no_moves_required=0);

    template <Color side>
    Score score_king_safety(const Position& position);

    Value combine(const Score& score);

    Value game_phase_weight(const Position& position);

    template <Color side>
    Bitboard blockers_for_square(const Position& position, Square sq,
                                 Bitboard& snipers);

    /*
     * Return bitboard will all "reasonable" moves from 'sq',
     *   i.e. don't move to squares with your pieces on it or
     *   don't moves to empty squares controlled by enemy pieces.
     * \in position - Evaulated position.
     * \in sq - Starting sqaure.
     * \in moves - Bitboard with all pseudo legal moves.
     * \out - Bitboard with "reasonable" moves.
     */
    template <Color side>
    Bitboard get_real_possible_moves(const Position& position, Square sq, Bitboard moves);

    PawnHashMap _pawn_hash_table;
    Value _weight;

    Bitboard _attacked_by_bb[COLOR_NUM][PIECE_KIND_NUM];
    Bitboard _attacked_by_piece[COLOR_NUM];
    Bitboard _outposts_bb[COLOR_NUM];

    Bitboard _blockers_for_king[COLOR_NUM];
    Bitboard _snipers_for_king[COLOR_NUM];

    Score _side_scores[COLOR_NUM];
    Score _piece_scores[COLOR_NUM][PIECE_KIND_NUM];

    std::pair<Piece, Score> _square_scores[SQUARE_NUM] = {std::make_pair(NO_PIECE, Score{})};
};

}  // namespace engine

#endif  // CHESS_ENGINE_SCORE_H_
