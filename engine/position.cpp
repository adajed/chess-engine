#include "position.h"

#include "bitboard.h"
#include "movegen.h"
#include "types.h"
#include "utils.h"
#include "zobrist_hash.h"

#include <functional>
#include <optional>
#include <sstream>

namespace engine
{
const std::regex Position::SAN_REGEX = std::regex(
    "([NBRQK]?)([a-h]?)([1-8]?)x?([a-h][1-8])=?([nbrqkNBRQK]?)[\\+#]?");

Square notationToSquare(std::string notation)
{
    File file = File(notation[0] - 'a');
    Rank rank = Rank(notation[1] - '1');
    return make_square(rank, file);
}

std::string squareToNotation(Square sq)
{
    const std::string file_str = "abcdefgh";
    const std::string rank_str = "12345678";
    std::string s = "";
    s += file_str[file(sq)];
    s += rank_str[rank(sq)];
    return s;
}

template <typename T>
std::vector<T> filter(std::vector<T> xs, std::function<bool(T)> pred)
{
    std::vector<T> ys;
    for (const T& x : xs)
    {
        if (pred(x)) ys.push_back(x);
    }
    return ys;
}

const std::string Position::STARTPOS_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

Position::Position() : Position(STARTPOS_FEN) {}

Position::Position(std::string fen) : _zobrist_hash()
{
    _current_side = WHITE;
    std::fill_n(_board, SQUARE_NUM, NO_PIECE);
    std::fill_n(_piece_count, PIECE_NUM, 0);
    std::fill_n(_by_piece_kind_bb, PIECE_KIND_NUM, 0ULL);
    std::fill_n(_by_color_bb, COLOR_NUM, 0ULL);
    _castling_rights = NO_CASTLING;
    set_enpassant_square(NO_SQUARE);

    std::istringstream stream(fen);
    std::string token;

    std::map<char, Piece> char_to_piece = {
        {'P', W_PAWN},   {'N', W_KNIGHT}, {'B', W_BISHOP}, {'R', W_ROOK},
        {'Q', W_QUEEN},  {'K', W_KING},   {'p', B_PAWN},   {'n', B_KNIGHT},
        {'b', B_BISHOP}, {'r', B_ROOK},   {'q', B_QUEEN},  {'k', B_KING},
    };

    stream >> token;
    Square square = SQ_A8;
    for (char c : token)
    {
        if (c == '/')
            square -= 16;
        else if ('0' <= c && c <= '9')
            square += (c - '0');
        else
        {
            Piece piece = char_to_piece[c];
            _board[square] = piece;
            _by_color_bb[get_color(piece)] |= square_bb(square);
            _by_piece_kind_bb[get_piece_kind(piece)] |= square_bb(square);
            _piece_position[piece][_piece_count[piece]++] = square;

            ++square;
        }
    }

    stream >> token;
    _current_side = token == "w" ? WHITE : BLACK;

    stream >> token;
    for (char c : token)
    {
        switch (c)
        {
        case 'K': _castling_rights |= W_OO; break;
        case 'Q': _castling_rights |= W_OOO; break;
        case 'k': _castling_rights |= B_OO; break;
        case 'q': _castling_rights |= B_OOO; break;
        }
    }

    stream >> token;
    set_enpassant_square(token == "-" ? NO_SQUARE : notationToSquare(token));

    stream >> _ply_counter;
    _half_move_counter = uint8_t(_ply_counter);
    stream >> _ply_counter;

    _ply_counter = 2 * _ply_counter - 1 + !!(_current_side == BLACK);

    _zobrist_hash.init(*this);

    _history[0] = _zobrist_hash.get_key();
    _history_counter = 1;
}

Position::Position(const std::vector<std::pair<Piece, Square>>& pieces) : _zobrist_hash()
{
    _current_side = WHITE;
    std::fill_n(_board, SQUARE_NUM, NO_PIECE);
    std::fill_n(_piece_count, PIECE_NUM, 0);
    std::fill_n(_by_piece_kind_bb, PIECE_KIND_NUM, 0ULL);
    std::fill_n(_by_color_bb, COLOR_NUM, 0ULL);
    _castling_rights = NO_CASTLING;
    set_enpassant_square(NO_SQUARE);

    for (const auto& [piece, sq] : pieces)
    {
        _board[sq] = piece;
        _by_color_bb[get_color(piece)] |= square_bb(sq);
        _by_piece_kind_bb[get_piece_kind(piece)] |= square_bb(sq);
        _piece_position[piece][_piece_count[piece]++] = sq;

    }

    _half_move_counter = 0;
    _ply_counter = 1;

    _zobrist_hash.init(*this);

    _history[0] = _zobrist_hash.get_key();
    _history_counter = 1;
}

bool Position::operator==(const Position& other) const
{
    // first check hashes
    if (_zobrist_hash.get_key() != other._zobrist_hash.get_key()) return false;

    if (_current_side != other._current_side) return false;
    if (_castling_rights != other._castling_rights) return false;
    if (_enpassant_square != other._enpassant_square) return false;
    for (Square square = SQ_A1; square <= SQ_H8; ++square)
        if (_board[square] != other._board[square]) return false;
    return true;
}

std::string Position::fen() const
{
    const std::string piece_to_char = " PNBRQKpnbrqk";

    std::ostringstream stream;

    int counter = 0;
    for (Rank rank = RANK_8; rank >= RANK_1; --rank)
    {
        for (File file = FILE_A; file <= FILE_H; ++file)
        {
            Piece piece = piece_at(make_square(rank, file));
            if (piece == NO_PIECE)
                counter++;
            else
            {
                if (counter > 0)
                {
                    stream << static_cast<char>('0' + counter);
                    counter = 0;
                }
                stream << piece_to_char[piece];
            }
        }
        if (counter > 0)
        {
            stream << static_cast<char>('0' + counter);
            counter = 0;
        }
        if (rank > 0) stream << "/";
    }
    stream << " " << (_current_side == WHITE ? "w" : "b") << " ";
    if (_castling_rights != NO_CASTLING)
    {
        if (_castling_rights & W_OO) stream << "K";
        if (_castling_rights & W_OOO) stream << "Q";
        if (_castling_rights & B_OO) stream << "k";
        if (_castling_rights & B_OOO) stream << "q";
    }
    else
        stream << "-";
    stream << " ";
    if (_enpassant_square != NO_SQUARE)
    {
        stream << char('a' + file(_enpassant_square));
        stream << char('1' + rank(_enpassant_square));
    }
    else
        stream << "-";

    stream << " " << uint32_t(_half_move_counter) << " "
           << (_ply_counter - 1) / 2 + 1;

    return stream.str();
}

bool Position::is_draw() const
{
    return rule50() || threefold_repetition() || !enough_material();
}

bool Position::threefold_repetition() const
{
    int count = 1;
    for (int i = _history_counter - 2; i >= 0; --i)
        if (_history[i] == _zobrist_hash.get_key())
            if (++count == 3) return true;
    return false;
}

bool Position::is_repeated() const
{
    for (int i = _history_counter - 2; i >= 0; --i)
        if (_history[i] == _zobrist_hash.get_key()) return true;
    return false;
}

bool Position::rule50() const
{
    return int(_half_move_counter) >= 100;
}

bool Position::enough_material() const
{
    constexpr PieceCountVector notEnoughMaterialPCV[] = {
        create_pcv(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), // only kings
        create_pcv(0, 0, 0, 0, 0, 0, 1, 0, 0, 0), // black knight
        create_pcv(0, 0, 0, 0, 0, 0, 0, 1, 0, 0), // black bishop
        create_pcv(0, 1, 0, 0, 0, 0, 0, 0, 0, 0), // white knight
        create_pcv(0, 0, 1, 0, 0, 0, 0, 0, 0, 0), // white bishop
    };

    return std::find(std::begin(notEnoughMaterialPCV),
                     std::end(notEnoughMaterialPCV),
                     get_pcv()) == std::end(notEnoughMaterialPCV);
}

bool Position::is_legal() const
{
    if (   _piece_count[W_KING] != 1
        || _piece_count[B_KING] != 1
        || KING_MASK[_piece_position[W_KING][0]] & square_bb(_piece_position[B_KING][0])
        || (color() == WHITE && is_in_check(BLACK))
        || (color() == BLACK && is_in_check(WHITE))
        )
        return false;
    return true;
}

bool Position::move_is_quiet(Move move) const
{
    if (castling(move) != NO_CASTLING)
        return true;

    if (promotion(move) != NO_PIECE_KIND)
        return false;

    if (to(move) == enpassant_square() && make_piece_kind(piece_at(from(move))) == PAWN)
        return false;

    if (piece_at(to(move)) == NO_PIECE)
        return true;

    return false;
}

bool Position::move_is_capture(Move move) const
{
    return castling(move) == NO_CASTLING
        && (piece_at(to(move)) != NO_PIECE
                || (make_piece_kind(piece_at(from(move))) == PAWN
                        && to(move) == enpassant_square()));
}

bool Position::move_gives_check(Move move) const
{
    const Square king_sq = piece_position(make_piece(!color(), KING));
    const Bitboard king_bb = square_bb(king_sq);
    Bitboard blockers = pieces();

    // castling
    if (castling(move) != NO_CASTLING)
    {
        Square old_king_sq = piece_position(make_piece(color(), KING));
        Square old_rook_sq = make_square(color() == WHITE ? RANK_1 : RANK_8,
                                        castling(move) & KING_CASTLING ? FILE_H : FILE_A);
        Square my_king_sq = make_square(color() == WHITE ? RANK_1 : RANK_8,
                                        castling(move) & KING_CASTLING ? FILE_G : FILE_C);
        Square my_rook_sq = make_square(color() == WHITE ? RANK_1 : RANK_8,
                                        castling(move) & KING_CASTLING ? FILE_F : FILE_D);

        blockers = pieces() ^ square_bb(old_king_sq) ^ square_bb(old_rook_sq) ^ square_bb(my_king_sq) ^ square_bb(my_rook_sq);
        if (slider_attack<ROOK>(my_rook_sq, blockers) & king_bb)
            return true;
    }

    const Square from_sq = from(move);
    const Square to_sq = to(move);
    const PieceKind moved_piece_kind = make_piece_kind(piece_at(from_sq));
    const Bitboard from_bb = square_bb(from_sq);
    const Bitboard to_bb = square_bb(to_sq);

    // direct check
    switch (moved_piece_kind)
    {
        case PAWN:
            if (pawn_attacks(square_bb(to_sq), color()) & king_bb)
                return true;
            break;
        case KNIGHT:
            if (KNIGHT_MASK[to_sq] & square_bb(king_sq))
                return true;
            break;
        case BISHOP:
            if (slider_attack<BISHOP>(to_sq, blockers) & king_bb)
                return true;
            break;
        case ROOK:
            if (slider_attack<ROOK>(to_sq, blockers) & king_bb)
                return true;
            break;
        case QUEEN:
            if (slider_attack<QUEEN>(to_sq, blockers) & king_bb)
                return true;
            break;
        case KING: break;
        default: assert(false);
    }

    blockers = (blockers ^ from_bb) | to_bb;

    // discovered check
    if (slider_attack<BISHOP>(king_sq, blockers) & pieces(color(), BISHOP, QUEEN))
        return true;
    if (slider_attack<ROOK>(king_sq, blockers) & pieces(color(), ROOK, QUEEN))
        return true;

    // enpassant
    if (moved_piece_kind == PAWN && to_sq == enpassant_square())
    {
        const Bitboard captured_bb = square_bb(make_square(rank(from_sq), file(to_sq)));
        blockers = blockers ^ captured_bb;

        if (slider_attack<BISHOP>(king_sq, blockers) & pieces(color(), BISHOP, QUEEN))
            return true;
        if (slider_attack<ROOK>(king_sq, blockers) & pieces(color(), ROOK, QUEEN))
            return true;
    }

    return false;
}

Bitboard Position::pieces() const
{
    return _by_color_bb[WHITE] | _by_color_bb[BLACK];
}

Bitboard Position::pieces(Color c) const
{
    return _by_color_bb[c];
}

Bitboard Position::pieces(PieceKind p) const
{
    return _by_piece_kind_bb[p];
}

Bitboard Position::pieces(Color c, PieceKind p) const
{
    return _by_color_bb[c] & _by_piece_kind_bb[p];
}

Bitboard Position::pieces(Piece p) const
{
    return pieces(get_color(p), get_piece_kind(p));
}

Bitboard Position::pieces(Piece p1, Piece p2) const
{
    return pieces(p1) | pieces(p2);
}

Bitboard Position::pieces(Color c, PieceKind p1, PieceKind p2) const
{
    return pieces(c, p1) | pieces(c, p2);
}

void Position::add_piece(Piece piece, Square square)
{
    ASSERT(_board[square] == NO_PIECE);

    _board[square] = piece;
    _by_color_bb[get_color(piece)] |= square_bb(square);
    _by_piece_kind_bb[get_piece_kind(piece)] |= square_bb(square);
    _piece_position[piece][_piece_count[piece]] = square;
    _piece_count[piece] += 1;

    _zobrist_hash.toggle_piece(piece, square);
}

void Position::remove_piece(Square square)
{
    ASSERT(_board[square] != NO_PIECE);

    Piece piece = _board[square];
    _board[square] = NO_PIECE;
    _by_color_bb[get_color(piece)] ^= square_bb(square);
    _by_piece_kind_bb[get_piece_kind(piece)] ^= square_bb(square);

    for (int i = 0; i < _piece_count[piece] - 1; ++i)
    {
        if (_piece_position[piece][i] == square)
        {
            int pos = _piece_count[piece] - 1;
            _piece_position[piece][i] = _piece_position[piece][pos];
            break;
        }
    }
    _piece_count[piece] -= 1;

    _zobrist_hash.toggle_piece(piece, square);
}

void Position::move_piece(Square from, Square to)
{
    ASSERT_WITH_MSG(_board[from] != NO_PIECE, "There is no piece at %d", from);
    ASSERT_WITH_MSG(_board[to] == NO_PIECE, "Piece at %d is %d", to, _board[to]);

    Piece piece = _board[from];
    _board[from] = NO_PIECE;
    _board[to] = piece;

    Bitboard change = square_bb(from) | square_bb(to);
    _by_color_bb[get_color(piece)] ^= change;
    _by_piece_kind_bb[get_piece_kind(piece)] ^= change;

    for (int i = 0; i < _piece_count[piece]; ++i)
    {
        if (_piece_position[piece][i] == from)
        {
            _piece_position[piece][i] = to;
            break;
        }
    }

    _zobrist_hash.move_piece(piece, from, to);
}

void Position::change_current_side()
{
    _zobrist_hash.flip_side();
    _current_side = !_current_side;
}

MoveInfo Position::do_move(Move move)
{
    Color side = _current_side;
    change_current_side();
    _ply_counter++;

    PieceKind captured = NO_PIECE_KIND;
    Castling prev_castling = _castling_rights;
    Square prev_enpassant_sq = _enpassant_square;
    bool enpassant = false;
    uint8_t hm_counter = _half_move_counter;

    _zobrist_hash.clear_enpassant();

    if (castling(move) != NO_CASTLING)
    {
        _half_move_counter = 0;

        Rank rank = side == WHITE ? RANK_1 : RANK_8;
        if (castling(move) == KING_CASTLING)
        {
            move_piece(make_square(rank, FILE_E), make_square(rank, FILE_G));
            move_piece(make_square(rank, FILE_H), make_square(rank, FILE_F));
        }
        else  // castling(move) == QUEEN_CASTLING
        {
            move_piece(make_square(rank, FILE_E), make_square(rank, FILE_C));
            move_piece(make_square(rank, FILE_A), make_square(rank, FILE_D));
        }

        _castling_rights &= !CASTLING_RIGHTS[side];
        _zobrist_hash.set_castling(_castling_rights);
        set_enpassant_square(NO_SQUARE);
    }
    else
    {
        Piece moved_piece = _board[from(move)];
        Piece captured_piece = _board[to(move)];
        captured = make_piece_kind(captured_piece);

        ASSERT(moved_piece != NO_PIECE);

        if (get_piece_kind(moved_piece) != PAWN &&
            make_piece_kind(captured_piece) == NO_PIECE_KIND)
            _half_move_counter++;
        else
            _half_move_counter = 0;

        // enpassant
        if (get_piece_kind(moved_piece) == PAWN &&
            to(move) == _enpassant_square)
        {
            move_piece(from(move), to(move));
            Square captured_square =
                Square(to(move) + (side == WHITE ? -8 : 8));
            remove_piece(captured_square);
            enpassant = true;
        }
        else
        {
            if (captured_piece != NO_PIECE) remove_piece(to(move));

            if (promotion(move) != NO_PIECE_KIND)
            {
                remove_piece(from(move));
                add_piece(make_piece(side, promotion(move)), to(move));
            }
            else
                move_piece(from(move), to(move));

            if (get_piece_kind(moved_piece) == KING)
                _castling_rights &= !CASTLING_RIGHTS[side];
            if (get_piece_kind(moved_piece) == ROOK &&
                from(move) == KING_SIDE_ROOK_SQUARE[side])
                _castling_rights &= !(CASTLING_RIGHTS[side] & KING_CASTLING);
            if (get_piece_kind(moved_piece) == ROOK &&
                from(move) == QUEEN_SIDE_ROOK_SQUARE[side])
                _castling_rights &= !(CASTLING_RIGHTS[side] & QUEEN_CASTLING);
            if (make_piece_kind(captured_piece) == ROOK &&
                to(move) == KING_SIDE_ROOK_SQUARE[!side])
                _castling_rights &= !(CASTLING_RIGHTS[!side] & KING_CASTLING);
            if (make_piece_kind(captured_piece) == ROOK &&
                to(move) == QUEEN_SIDE_ROOK_SQUARE[!side])
                _castling_rights &= !(CASTLING_RIGHTS[!side] & QUEEN_CASTLING);
            _zobrist_hash.set_castling(_castling_rights);
        }

        Rank enpassant_rank = (side == WHITE) ? RANK_4 : RANK_5;
        Rank rank2 = (side == WHITE) ? RANK_2 : RANK_7;
        if (get_piece_kind(moved_piece) == PAWN && rank(from(move)) == rank2 &&
            rank(to(move)) == enpassant_rank)
        {
            set_enpassant_square(Square(to(move) + (side == WHITE ? -8 : 8)));
            _zobrist_hash.set_enpassant(file(_enpassant_square));
        }
        else
            set_enpassant_square(NO_SQUARE);
    }

    assert(_history_counter < MAX_PLIES);
    _history[_history_counter++] = _zobrist_hash.get_key();

    return create_moveinfo(captured, prev_castling, prev_enpassant_sq,
                           enpassant, hm_counter);
}

void Position::undo_move(Move move, MoveInfo moveinfo)
{
    change_current_side();
    Color side = _current_side;

    _ply_counter--;

    _castling_rights = last_castling(moveinfo);
    _zobrist_hash.set_castling(_castling_rights);

    set_enpassant_square(last_enpassant_square(moveinfo));
    if (_enpassant_square == NO_SQUARE)
        _zobrist_hash.clear_enpassant();
    else
        _zobrist_hash.set_enpassant(file(_enpassant_square));

    _half_move_counter = half_move_counter(moveinfo);

    if (castling(move) != NO_CASTLING)
    {
        Rank rank = side == WHITE ? RANK_1 : RANK_8;
        if (castling(move) == KING_CASTLING)
        {
            move_piece(make_square(rank, FILE_G), make_square(rank, FILE_E));
            move_piece(make_square(rank, FILE_F), make_square(rank, FILE_H));
        }
        else  // castling(move) == QUEEN_CASTLING
        {
            move_piece(make_square(rank, FILE_C), make_square(rank, FILE_E));
            move_piece(make_square(rank, FILE_D), make_square(rank, FILE_A));
        }
    }
    else
    {
        Piece captured = make_piece(!side, captured_piece(moveinfo));

        if (enpassant(moveinfo))
        {
            Square captured_square =
                Square(to(move) + (side == WHITE ? -8 : 8));
            add_piece(make_piece(!side, PAWN), captured_square);
        }

        if (promotion(move) != NO_PIECE_KIND)
        {
            add_piece(make_piece(side, PAWN), from(move));
            remove_piece(to(move));
        }
        else
            move_piece(to(move), from(move));

        if (captured != NO_PIECE) add_piece(captured, to(move));
    }

    _history_counter--;
}

void Position::set_enpassant_square(Square sq)
{
    ASSERT(sq == NO_SQUARE || rank(sq) == RANK_3 || rank(sq) == RANK_6);
    _enpassant_square = sq;
}

MoveInfo Position::do_null_move()
{
    change_current_side();
    _ply_counter++;
    _half_move_counter++;

    Square enpassant_sq = _enpassant_square;
    set_enpassant_square(NO_SQUARE);
    _zobrist_hash.clear_enpassant();

    return create_moveinfo(NO_PIECE_KIND, NO_CASTLING, enpassant_sq, false, 0);
}

void Position::undo_null_move(MoveInfo moveinfo)
{
    change_current_side();
    _ply_counter--;
    _half_move_counter--;

    set_enpassant_square(last_enpassant_square(moveinfo));
    if (_enpassant_square != NO_SQUARE)
        _zobrist_hash.set_enpassant(file(_enpassant_square));
}

bool Position::is_in_check(Color side) const
{
    const Square king_sq = piece_position(make_piece(side, KING));

    if (pawn_attacks(square_bb(king_sq), side) & pieces(!side, PAWN))
        return true;
    if (KNIGHT_MASK[king_sq] & pieces(!side, KNIGHT))
        return true;
    if (slider_attack<BISHOP>(king_sq, pieces()) & pieces(!side, BISHOP, QUEEN))
        return true;
    if (slider_attack<ROOK>(king_sq, pieces()) & pieces(!side, ROOK, QUEEN))
        return true;

    return false;
}

bool Position::is_checkmate() const
{
    Move* begin = MOVE_LIST[0];
    Move* end = generate_moves(*this, _current_side, begin);

    return (begin == end) && is_in_check(_current_side);
}

bool Position::is_stalemate() const
{
    Move* begin = MOVE_LIST[0];
    Move* end = generate_moves(*this, _current_side, begin);

    return (begin == end) && !is_in_check(_current_side);
}

PieceCountVector Position::get_pcv() const
{
    return create_pcv(_piece_count[W_PAWN], _piece_count[W_KNIGHT],
                      _piece_count[W_BISHOP], _piece_count[W_ROOK],
                      _piece_count[W_QUEEN], _piece_count[B_PAWN],
                      _piece_count[B_KNIGHT], _piece_count[B_BISHOP],
                      _piece_count[B_ROOK], _piece_count[B_QUEEN]);
}

Value Position::see(Move move) const
{
    Square from_sq = from(move);
    Square to_sq = to(move);
    Color side = color();
    PieceKind current_piecekind = get_piece_kind(piece_at(from_sq));

    PieceKind pks[32];
    int counter = 0;

    pks[counter++] = get_piece_kind(piece_at(to_sq));

    Bitboard occupied = pieces() & ~(square_bb(from_sq) | square_bb(to_sq));
    Bitboard attackers[] = {
        square_attackers(to_sq, WHITE),
        square_attackers(to_sq, BLACK)
    };

    attackers[side] &= ~square_bb(from_sq);

    while (true)
    {
        side = !side;

        if (!attackers[side])
        {
            break;
        }

        for (PieceKind pk : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
        {
            Bitboard temp = attackers[side] & pieces(side, pk) & occupied;
            if (temp)
            {
                Square sq = Square(pop_lsb(&temp));
                attackers[side] &= ~square_bb(sq);
                occupied &= ~square_bb(sq);
                pks[counter++] = current_piecekind;
                current_piecekind = pk;
                break;
            }
        }
    }

    Value value = 0;
    counter--;
    for(; counter > 0; --counter)
    {
        value = std::max(Value(0), PIECE_VALUE[pks[counter]].eg - value);
    }
    // force first capture
    value = PIECE_VALUE[pks[0]].eg - value;
    return value;
}

Bitboard Position::square_attackers(Square sq, Color color) const
{
    Bitboard attackers = no_squares_bb;

    attackers |= pawn_attacks(square_bb(sq), !color) & pieces(color, PAWN);
    attackers |= KNIGHT_MASK[sq] & pieces(color, KNIGHT);
    attackers |= slider_attack<BISHOP>(sq, pieces()) & pieces(color, BISHOP, QUEEN);
    attackers |= slider_attack<ROOK>(sq, pieces()) & pieces(color, ROOK, QUEEN);
    attackers |= KING_MASK[sq] & pieces(color, KING);

    return attackers;
}

int Position::no_nonpawns(Color c) const
{
    return _piece_count[make_piece(c, KNIGHT)] +
           _piece_count[make_piece(c, BISHOP)] +
           _piece_count[make_piece(c, ROOK)] +
           _piece_count[make_piece(c, QUEEN)];
}

std::string Position::uci(Move move) const
{
    const std::string files = "abcdefgh";
    const std::string ranks = "12345678";
    const std::string promotions = "  nbrq ";

    if (castling(move) & KING_CASTLING)
        return _current_side == WHITE ? "e1g1" : "e8g8";
    if (castling(move) & QUEEN_CASTLING)
        return _current_side == WHITE ? "e1c1" : "e8c8";

    std::string str = "";
    str += files[file(from(move))];
    str += ranks[rank(from(move))];
    str += files[file(to(move))];
    str += ranks[rank(to(move))];

    if (promotion(move) != NO_PIECE_KIND) str += promotions[promotion(move)];

    return str;
}

Move Position::parse_uci(const std::string& str)
{
    Move move = NO_MOVE;
    Square from = make_square(Rank(str[1] - '1'), File(str[0] - 'a'));
    Square to = make_square(Rank(str[3] - '1'), File(str[2] - 'a'));
    PieceKind promotion = NO_PIECE_KIND;

    if (str.size() > 4)
    {
        switch (str[4])
        {
        case 'n':
        case 'N':
            promotion = KNIGHT; break;
        case 'b':
        case 'B':
            promotion = BISHOP; break;
        case 'r':
        case 'R':
            promotion = ROOK; break;
        case 'q':
        case 'Q':
            promotion = QUEEN; break;
        default:
            throw std::runtime_error("Unknown promotion piece: " + str.substr(4));
        }
    }

    move = create_promotion(from, to, promotion);

    if (make_piece_kind(_board[from]) == KING && from == SQ_E1 && to == SQ_G1)
        move = create_castling(KING_CASTLING);
    if (make_piece_kind(_board[from]) == KING && from == SQ_E1 && to == SQ_C1)
        move = create_castling(QUEEN_CASTLING);
    if (make_piece_kind(_board[from]) == KING && from == SQ_E8 && to == SQ_G8)
        move = create_castling(KING_CASTLING);
    if (make_piece_kind(_board[from]) == KING && from == SQ_E8 && to == SQ_C8)
        move = create_castling(QUEEN_CASTLING);

    return move;
}

Move Position::parse_san(const std::string& str)
{
    Move movelist[MAX_MOVES];
    Move* begin = movelist;
    Move* end = generate_moves(*this, color(), begin);

    if (str == "0-0" || str == "O-O")
        return std::find(begin, end, KING_CASTLING_MOVE) != end ? KING_CASTLING_MOVE : NO_MOVE;
    if (str == "0-0-0" || str == "O-O-O")
        return std::find(begin, end, QUEEN_CASTLING_MOVE) != end ? QUEEN_CASTLING_MOVE : NO_MOVE;

    std::smatch match;
    if (!std::regex_match(str, match, SAN_REGEX))
        return NO_MOVE;

     auto parse_piece_kind = [](std::string s) {
         if (s == "n" || s == "N") return KNIGHT;
         if (s == "b" || s == "B") return BISHOP;
         if (s == "r" || s == "R") return ROOK;
         if (s == "q" || s == "Q") return QUEEN;
         if (s == "k" || s == "K") return KING;
         return PAWN;
     };

    PieceKind moved_piece = parse_piece_kind(match[1]);
    std::optional<File> from_file;
    if (match[2].length() > 0) from_file = File(match[2].str().at(0) - 'a');
    std::optional<Rank> from_rank;
    if (match[3].length() > 0) from_rank = Rank(match[3].str().at(0) - '1');
    Square to_square = make_square(Rank(match[4].str().at(1) - '1'), File(match[4].str().at(0) - 'a'));
    std::optional<PieceKind> promotion_piece_kind;
    if (match[5].length() > 0) promotion_piece_kind = parse_piece_kind(match[5]);

    if (promotion_piece_kind &&
            (promotion_piece_kind == PAWN || promotion_piece_kind == KING))
        return NO_MOVE;

    Move matching_move = NO_MOVE;
    int matching_move_count = 0;
    for (Move* it = begin; it != end; ++it)
    {
        Move move = *it;
        if (make_piece_kind(piece_at(from(move))) == moved_piece &&
                (!from_file || file(from(move)) == from_file.value()) &&
                (!from_rank || rank(from(move)) == from_rank.value()) &&
                to(move) == to_square &&
                (!promotion_piece_kind || promotion(move) == promotion_piece_kind))
        {
            matching_move = move;
            matching_move_count++;
        }
    }

    if (matching_move_count != 1)
        return NO_MOVE;

    return matching_move;
}

std::string Position::san(Move move) const
{
    std::string basic_san = san_without_check(move);
    Position temp = *this;
    temp.do_move(move);

    if (temp.is_checkmate()) return basic_san + "#";
    if (temp.is_in_check(temp.color())) return basic_san + "+";
    return basic_san;
}

std::string Position::san_without_check(Move move) const
{
    /* assert(is_legal(move)); */
    const std::string piece_str = "  NBRQK";
    const std::string rank_str = "12345678";
    const std::string file_str = "abcdefgh";
    const std::string promotion_str = "  NBRQ ";

    if (castling(move) == KING_CASTLING) return "O-O";
    if (castling(move) == QUEEN_CASTLING) return "O-O-O";

    PieceKind moved_piece = make_piece_kind(piece_at(from(move)));

    std::array<Move, 128> moves;
    Move* begin = moves.data();
    Move* end = generate_moves(*this, _current_side, begin);
    std::vector<Move> matching_moves(begin, end);

    matching_moves =
        filter<Move>(matching_moves, [this, move, moved_piece](Move m) {
            if (castling(m) == NO_CASTLING)
            {
                PieceKind p = make_piece_kind(piece_at(from(m)));
                return (moved_piece == p && to(move) == to(m) &&
                        promotion(move) == promotion(m));
            }
            return false;
        });

    std::string s = "";
    if (moved_piece != PAWN)
    {
        s += piece_str[moved_piece];
    }

    if (matching_moves.size() > 1)
    {
        s += file_str[file(from(move))];
        matching_moves = filter<Move>(matching_moves, [move](Move m) {
            return file(from(m)) == file(from(move));
        });
        if (matching_moves.size() > 1)
        {
            s += rank_str[rank(from(move))];
        }
    }

    Bitboard capturing_bb = pieces(!_current_side);
    capturing_bb |=
        moved_piece == PAWN ? square_bb(_enpassant_square) : no_squares_bb;
    if (square_bb(to(move)) & capturing_bb)
    {
        if (moved_piece == PAWN && s == "")
        {
            s += file_str[file(from(move))];
        }
        s += "x";
    }

    s += squareToNotation(to(move));
    if (promotion(move) != NO_PIECE_KIND)
    {
        s += "=";
        s += promotion_str[promotion(move)];
    }

    return s;
}

std::ostream& operator<<(std::ostream& stream, const Position& position)
{
    const std::string piece_to_char = ".PNBRQKpnbrqk";

    for (Rank rank = RANK_8; rank >= RANK_1; --rank)
    {
        stream << (rank + 1) << "  ";
        for (File file = FILE_A; file <= FILE_H; ++file)
        {
            Piece piece = position.piece_at(make_square(rank, file));
            stream << piece_to_char[piece] << " ";
        }
        stream << std::endl;
    }
    stream << std::endl;
    stream << "   A B C D E F G H" << std::endl;
    stream << std::endl;
    stream << "Fen: \"" << position.fen() << "\"" << std::endl;
    stream << "Hash: " << std::hex << position.hash() << std::dec << std::endl;
    if (position.color() == WHITE)
        stream << "White to move" << std::endl;
    else
        stream << "Black to move" << std::endl;

    return stream;
}

}  // namespace engine
