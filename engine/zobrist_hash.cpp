#include "zobrist_hash.h"

#include "position.h"
#include "types.h"
#include "hash_position.h"

namespace engine
{
namespace zobrist
{

PieceCountVector build_pcv(const std::string& code, Color c)
{
    std::string codeWhite = code.substr(0, std::min(code.find('v'), code.find('K', 1)));
    std::string codeBlack = code.substr(code.find('K', 1));

    if (c == BLACK)
        std::swap(codeWhite, codeBlack);

    PieceCountVector pcv = 0ULL;

    pcv = modify_pcv(pcv, std::count(codeWhite.begin(), codeWhite.end(), 'P'), W_PAWN);
    pcv = modify_pcv(pcv, std::count(codeWhite.begin(), codeWhite.end(), 'N'), W_KNIGHT);
    pcv = modify_pcv(pcv, std::count(codeWhite.begin(), codeWhite.end(), 'B'), W_BISHOP);
    pcv = modify_pcv(pcv, std::count(codeWhite.begin(), codeWhite.end(), 'R'), W_ROOK);
    pcv = modify_pcv(pcv, std::count(codeWhite.begin(), codeWhite.end(), 'Q'), W_QUEEN);

    pcv = modify_pcv(pcv, std::count(codeBlack.begin(), codeBlack.end(), 'P'), B_PAWN);
    pcv = modify_pcv(pcv, std::count(codeBlack.begin(), codeBlack.end(), 'N'), B_KNIGHT);
    pcv = modify_pcv(pcv, std::count(codeBlack.begin(), codeBlack.end(), 'B'), B_BISHOP);
    pcv = modify_pcv(pcv, std::count(codeBlack.begin(), codeBlack.end(), 'R'), B_ROOK);
    pcv = modify_pcv(pcv, std::count(codeBlack.begin(), codeBlack.end(), 'Q'), B_QUEEN);

    return pcv;
}

}  // namespace zobrist

HashKey::HashKey()
    : _piece_key(0ULL),
      _pawn_key(0ULL),
      _enpassant_key(0ULL),
      _castling_key(0ULL),
      _color_key(0ULL)
{
}

HashKey::HashKey(PieceCountVector pcv)
    : _piece_key(0ULL),
      _pawn_key(0ULL),
      _enpassant_key(0ULL),
      _castling_key(0ULL),
      _color_key(0ULL)
{
    for (Color c : {WHITE, BLACK})
    {
        _piece_key ^= HASH_PIECE[make_piece(c, KING)][0];
        for (PieceKind pk : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN})
        {
            Piece p = make_piece(c, pk);
            int count = get_count_pcv(pcv, p);
            for (int i = 0; i < count; ++i)
                _piece_key ^= HASH_PIECE[p][i];
        }
    }
}

HashKey::HashKey(const std::string& code, Color c)
    : HashKey(zobrist::build_pcv(code, c))
{
}

void HashKey::init(const Position& position)
{
    if (position.color() == WHITE) _color_key ^= HASH_TURN;

    for (Color color : {WHITE, BLACK})
    {
        const Piece piece = make_piece(color, PAWN);
        for (int i = 0; i < position.no_pieces(piece); ++i)
        {
            const Square square = position.piece_position(piece, i);
            _pawn_key ^= HASH_PIECE[piece][square];
        }
    }

    for (Color color : {WHITE, BLACK})
    {
        for (PieceKind piece_kind : {KNIGHT, BISHOP, ROOK, QUEEN, KING})
        {
            const Piece piece = make_piece(color, piece_kind);
            for (int i = 0; i < position.no_pieces(piece); ++i)
            {
                const Square square = position.piece_position(piece, i);
                _piece_key ^= HASH_PIECE[piece][square];
            }
        }
    }

    set_castling(position.castling_rights());

    if (position.enpassant_square() != NO_SQUARE)
        set_enpassant(file(position.enpassant_square()));
}

uint64_t HashKey::get_key() const
{
    return _piece_key ^ _pawn_key ^ _enpassant_key ^ _castling_key ^ _color_key;
}

uint64_t HashKey::get_pawnkey() const
{
    return _pawn_key;
}

void HashKey::move_piece(Piece piece, Square from, Square to)
{
    toggle_piece(piece, from);
    toggle_piece(piece, to);
}

void HashKey::toggle_piece(Piece piece, Square sq)
{
    if (get_piece_kind(piece) == PAWN)
        _pawn_key ^= HASH_PIECE[piece][sq];
    else
        _piece_key ^= HASH_PIECE[piece][sq];
}

void HashKey::flip_side()
{
    _color_key ^= HASH_TURN;
}

void HashKey::clear_enpassant()
{
    _enpassant_key = 0ULL;
}

void HashKey::set_enpassant(File file)
{
    _enpassant_key = HASH_ENPASSANT[file];
}

void HashKey::clear_castling()
{
    _castling_key = 0ULL;
}

void HashKey::set_castling(Castling castling)
{
    clear_castling();
    if (castling & W_OO) _castling_key ^= HASH_CASTLING_WHITE_SHORT;
    if (castling & W_OOO) _castling_key ^= HASH_CASTLING_WHITE_LONG;
    if (castling & B_OO) _castling_key ^= HASH_CASTLING_BLACK_SHORT;
    if (castling & B_OOO) _castling_key ^= HASH_CASTLING_BLACK_LONG;
}

}  // namespace engine
