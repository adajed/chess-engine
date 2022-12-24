#include "position.h"

#include "movegen.h"
#include "types.h"
#include "utils.h"
#include "zobrist_hash.h"

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
    Bitboard attacked = attacked_squares(*this, side);
    return attacked & pieces(side, KING);
}

bool Position::is_checkmate() const
{
    Move* begin = MOVE_LIST[0];
    Move* end = generate_moves(*this, _current_side, begin);

    return (begin == end) && is_in_check(_current_side);
}

PieceCountVector Position::get_pcv() const
{
    return create_pcv(_piece_count[W_PAWN], _piece_count[W_KNIGHT],
                      _piece_count[W_BISHOP], _piece_count[W_ROOK],
                      _piece_count[W_QUEEN], _piece_count[B_PAWN],
                      _piece_count[B_KNIGHT], _piece_count[B_BISHOP],
                      _piece_count[B_ROOK], _piece_count[B_QUEEN]);
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
        case 'n': promotion = KNIGHT; break;
        case 'b': promotion = BISHOP; break;
        case 'r': promotion = ROOK; break;
        case 'q': promotion = QUEEN; break;
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
