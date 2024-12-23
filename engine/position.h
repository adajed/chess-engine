#ifndef CHESS_ENGINE_POSITION_H_
#define CHESS_ENGINE_POSITION_H_

#include "bitboard.h"
#include "move_bitboards.h"
#include "types.h"
#include "value.h"
#include "zobrist_hash.h"

#if defined(_WIN32)
#include <stdint.h>
#else
#include <bits/stdint-uintn.h>
#endif
#include <cassert>
#include <cinttypes>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace engine
{
class Position
{
  public:
    static const std::string STARTPOS_FEN;
    static const std::regex SAN_REGEX;

    explicit Position();
    explicit Position(std::string fen);
    Position(const std::vector<std::pair<Piece, Square>>& pieces);

    bool operator==(const Position& other) const;

    // generate fen string for position
    std::string fen() const;

    MoveInfo do_move(Move move);
    void undo_move(Move move, MoveInfo moveinfo);

    MoveInfo do_null_move();
    void undo_null_move(MoveInfo moveinfo);

    bool is_in_check(Color side) const;
    bool is_checkmate() const;
    bool is_stalemate() const;

    Color color() const { return _current_side; }

    uint32_t half_moves() const { return _half_move_counter; }
    uint32_t ply_count() const { return _ply_counter; }

    bool is_draw() const;
    bool threefold_repetition() const;
    bool rule50() const;
    bool enough_material() const;

    bool is_legal() const;

    bool move_is_quiet(Move move) const;
    bool move_is_capture(Move move) const;
    bool move_gives_check(Move move) const;

    /*
     * Checks if current position was ever reached
     * (faster then checking for threefold_repetition).
     */
    bool is_repeated() const;

    Piece piece_at(Square square) const { return _board[square]; }

    int no_pieces(Piece piece) const { return _piece_count[piece]; }
    int no_pieces(Color color, PieceKind pk) const { return no_pieces(make_piece(color, pk)); }

    Square piece_position(Piece piece, int pos = 0) const
    {
        return _piece_position[piece][pos];
    }
    Square piece_position(Color color, PieceKind pk, int pos = 0) const
    {
        return piece_position(make_piece(color, pk), pos);
    }

    Castling castling_rights() const { return _castling_rights; }
    Square enpassant_square() const { return _enpassant_square; }

    uint64_t hash() const { return _zobrist_hash.get_key(); }
    uint64_t pawn_hash() const { return _zobrist_hash.get_pawnkey(); }
    uint64_t material_hash() const { return HashKey(get_pcv()).get_key(); }

    Bitboard pieces() const;
    Bitboard pieces(Color c) const;
    Bitboard pieces(PieceKind p) const;
    Bitboard pieces(Color c, PieceKind p) const;
    Bitboard pieces(Piece p) const;
    Bitboard pieces(Piece p1, Piece p2) const;
    Bitboard pieces(Color c, PieceKind p1, PieceKind p2) const;

    PieceCountVector get_pcv() const;

    Value see(Move move) const;

    Bitboard square_attackers(Square sq, Color color) const;

    /*
     * Return number of non-pawn pieces for given side.
     */
    int no_nonpawns(Color c) const;

    Move parse_uci(const std::string& str);
    Move parse_san(const std::string& str);

    std::string uci(Move move) const;
    std::string san(Move move) const;

  private:
    void add_piece(Piece piece, Square square);
    void remove_piece(Square square);
    void move_piece(Square from, Square to);

    void change_current_side();
    void set_enpassant_square(Square sq);

    std::string san_without_check(Move move) const;

    Color _current_side;

    uint8_t _half_move_counter;
    int32_t _ply_counter;

    Piece _board[SQUARE_NUM];
    Square _piece_position[PIECE_NUM][10];
    int _piece_count[PIECE_NUM];

    Bitboard _by_piece_kind_bb[PIECE_KIND_NUM];
    Bitboard _by_color_bb[COLOR_NUM];

    Castling _castling_rights;
    Square _enpassant_square;

    HashKey _zobrist_hash;

    int32_t _history_counter;
    uint64_t _history[MAX_PLIES];
};

std::ostream& operator<<(std::ostream& stream, const Position& position);

}  // namespace engine

#endif  // CHESS_ENGINE_POSITION_H_
