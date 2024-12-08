#include "movegen.h"

#include "position.h"

namespace engine
{
Move MOVE_LIST[4 * MAX_DEPTH][MAX_MOVES];
Move TEMP_MOVE_LIST[MAX_MOVES];

Bitboard attack_in_ray(Square sq, Ray ray, Bitboard blockers)
{
    Bitboard masked_blockers = blockers & RAYS[ray][sq];
    if (!masked_blockers) return RAYS[ray][sq];
    Square blocker;
    if (RAY_NW <= ray && ray < RAY_SE)
        blocker = Square(lsb(masked_blockers));
    else
        blocker = Square(msb(masked_blockers));

    return RAYS[ray][sq] & ~RAYS[ray][blocker];
}

Bitboard attack_in_line(Square sq, Ray ray, Bitboard blockers)
{
    return attack_in_ray(sq, ray, blockers) |
           attack_in_ray(sq, opposite_ray(ray), blockers);
}

template <Color side>
Bitboard checkers(const Position& position)
{
    const Piece CURRENT_KING = side == WHITE ? W_KING : B_KING;
    const Direction UPLEFT = side == WHITE ? NORTHWEST : SOUTHEAST;
    const Direction UPRIGHT = side == WHITE ? NORTHEAST : SOUTHWEST;

    Square king_sq = position.piece_position(CURRENT_KING, 0);

    Bitboard bb = 0ULL;

    bb |= (shift<UPLEFT>(square_bb(king_sq)) |
           shift<UPRIGHT>(square_bb(king_sq))) &
          position.pieces(!side, PAWN);
    bb |= KNIGHT_MASK[king_sq] & position.pieces(!side, KNIGHT);
    bb |= slider_attack<BISHOP>(king_sq, position.pieces()) &
          (position.pieces(!side, BISHOP) | position.pieces(!side, QUEEN));
    bb |= slider_attack<ROOK>(king_sq, position.pieces()) &
          (position.pieces(!side, ROOK) | position.pieces(!side, QUEEN));

    return bb;
}

template <Color side>
Bitboard forbidden_squares(const Position& pos)
{
    const Direction UPLEFT = side == BLACK ? NORTHWEST : SOUTHEAST;
    const Direction UPRIGHT = side == BLACK ? NORTHEAST : SOUTHWEST;

    const Piece OPPONENT_KNIGHT = side == WHITE ? B_KNIGHT : W_KNIGHT;
    const Piece OPPONENT_BISHOP = side == WHITE ? B_BISHOP : W_BISHOP;
    const Piece OPPONENT_ROOK = side == WHITE ? B_ROOK : W_ROOK;
    const Piece OPPONENT_QUEEN = side == WHITE ? B_QUEEN : W_QUEEN;
    const Piece OPPONENT_KING = side == WHITE ? B_KING : W_KING;

    const Square king_sq = pos.piece_position(make_piece(side, KING), 0);

    Bitboard bb = 0ULL;

    Bitboard pawns = pos.pieces(!side, PAWN);
    bb |= shift<UPLEFT>(pawns) | shift<UPRIGHT>(pawns);

    for (int i = 0; i < pos.no_pieces(OPPONENT_KNIGHT); ++i)
        bb |= KNIGHT_MASK[pos.piece_position(OPPONENT_KNIGHT, i)];

    Bitboard blockers = pos.pieces() ^ square_bb(king_sq);
    for (int i = 0; i < pos.no_pieces(OPPONENT_BISHOP); ++i)
        bb |= slider_attack<BISHOP>(pos.piece_position(OPPONENT_BISHOP, i),
                                    blockers);
    for (int i = 0; i < pos.no_pieces(OPPONENT_ROOK); ++i)
        bb |=
            slider_attack<ROOK>(pos.piece_position(OPPONENT_ROOK, i), blockers);
    for (int i = 0; i < pos.no_pieces(OPPONENT_QUEEN); ++i)
        bb |= slider_attack<QUEEN>(pos.piece_position(OPPONENT_QUEEN, i),
                                   blockers);

    bb |= KING_MASK[pos.piece_position(OPPONENT_KING, 0)];

    return bb;
}

#define FOR_EACH_BIT(bitboard, code)            \
    while (bitboard)                            \
    {                                           \
        Square sq = Square(pop_lsb(&bitboard)); \
        code;                                   \
    }

// generate moves for not pinned pawns
template <Color side>
Move* generate_pawn_moves(Bitboard pawns, Bitboard empty, Bitboard push_mask,
                          Bitboard capture_mask, Move* list)
{
    const Direction UP = side == WHITE ? NORTH : SOUTH;
    const Direction UPRIGHT = side == WHITE ? NORTHEAST : SOUTHWEST;
    const Direction UPLEFT = side == WHITE ? NORTHWEST : SOUTHEAST;
    const Bitboard rank3 = side == WHITE ? rank3_bb : rank6_bb;
    const Bitboard rank7 = side == WHITE ? rank7_bb : rank2_bb;
    const int up = side == WHITE ? 8 : -8;
    const int upright = side == WHITE ? 9 : -9;
    const int upleft = side == WHITE ? 7 : -7;

    Bitboard pawnsOn7 = pawns & rank7;
    Bitboard pawnsNotOn7 = pawns & ~rank7;

    //// pawns on 7
    // captures to the right
    Bitboard bb = shift<UPRIGHT>(pawnsOn7) & capture_mask;
    FOR_EACH_BIT(bb,
                 *list++ = create_promotion(Square(sq - upright), sq, QUEEN);
                 *list++ = create_promotion(Square(sq - upright), sq, ROOK);
                 *list++ = create_promotion(Square(sq - upright), sq, BISHOP);
                 *list++ = create_promotion(Square(sq - upright), sq, KNIGHT);)

    // captures to the left
    bb = shift<UPLEFT>(pawnsOn7) & capture_mask;
    FOR_EACH_BIT(bb, *list++ = create_promotion(Square(sq - upleft), sq, QUEEN);
                 *list++ = create_promotion(Square(sq - upleft), sq, ROOK);
                 *list++ = create_promotion(Square(sq - upleft), sq, BISHOP);
                 *list++ = create_promotion(Square(sq - upleft), sq, KNIGHT);)

    // moves
    bb = shift<UP>(pawnsOn7) & push_mask & empty;
    FOR_EACH_BIT(bb, *list++ = create_promotion(Square(sq - up), sq, QUEEN);
                 *list++ = create_promotion(Square(sq - up), sq, ROOK);
                 *list++ = create_promotion(Square(sq - up), sq, BISHOP);
                 *list++ = create_promotion(Square(sq - up), sq, KNIGHT);)

    //// pawns not on 7
    // captures to the right
    bb = shift<UPRIGHT>(pawnsNotOn7) & capture_mask;
    FOR_EACH_BIT(bb, *list++ = create_move(Square(sq - upright), sq))

    // captures to the left
    bb = shift<UPLEFT>(pawnsNotOn7) & capture_mask;
    FOR_EACH_BIT(bb, *list++ = create_move(Square(sq - upleft), sq))

    // moves
    Bitboard pushed_pawns = shift<UP>(pawnsNotOn7) & empty;
    bb = pushed_pawns & push_mask;
    FOR_EACH_BIT(bb, *list++ = create_move(Square(sq - up), sq))
    bb = shift<UP>(pushed_pawns & rank3) & push_mask & empty;
    FOR_EACH_BIT(bb, *list++ = create_move(Square(sq - 2 * up), sq))

    return list;
}

template <Color side>
Move* generate_enpassant(const Position& pos, Bitboard not_pinned_pawns,
                         Bitboard push_mask, Bitboard capture_mask,
                         Square enpassant_square, Move* list)
{
    const Direction UPRIGHT = side == WHITE ? NORTHEAST : SOUTHWEST;
    const Direction UPLEFT = side == WHITE ? NORTHWEST : SOUTHEAST;
    const int up = side == WHITE ? 8 : -8;
    const int upright = side == WHITE ? 9 : -9;
    const int upleft = side == WHITE ? 7 : -7;

    Square captured_square = Square(enpassant_square - up);

    if (!(square_bb(captured_square) & capture_mask ||
          square_bb(enpassant_square) & push_mask))
        return list;

    Bitboard right_bb =
        shift<UPRIGHT>(not_pinned_pawns) & square_bb(enpassant_square);
    Bitboard left_bb =
        shift<UPLEFT>(not_pinned_pawns) & square_bb(enpassant_square);

    if (bool(right_bb) != bool(left_bb))
    {
        Bitboard attacking_bb = 0ULL;
        if (right_bb)
            attacking_bb = square_bb(Square(enpassant_square - upright));
        if (left_bb)
            attacking_bb = square_bb(Square(enpassant_square - upleft));
        Bitboard blockers =
            pos.pieces() ^ (square_bb(captured_square) | attacking_bb);
        Square king_square = pos.piece_position(make_piece(side, KING), 0);
        Bitboard attacked_bb = attack_in_line(king_square, RAY_E, blockers);

        if (attacked_bb & (pos.pieces(!side, ROOK) | pos.pieces(!side, QUEEN)))
            return list;
    }

    if (right_bb)
        *list++ =
            create_move(Square(enpassant_square - upright), enpassant_square);
    if (left_bb)
        *list++ =
            create_move(Square(enpassant_square - upleft), enpassant_square);

    return list;
}

// 0-5 square
// 6-8 piece kind
// 9-11 ray
using Pin = uint32_t;

Pin PINS[MAX_PINS];

Pin create_pin(Square square, PieceKind piece, Ray ray)
{
    return ray << 9 | piece << 6 | square;
}

Square pin_square(Pin pin)
{
    return Square(pin & 0x3F);
}

PieceKind pin_piece_kind(Pin pin)
{
    return PieceKind((pin >> 6) & 0x7);
}

Ray pin_ray(Pin pin)
{
    return Ray((pin >> 9) & 0x7);
}

// generates all moves for a given piece,
//  assumes that king is not in check
//  and that piece is not pinned
template <PieceKind piece>
Move* generate_piece_moves(Square from, const Position& pos, Bitboard target,
                           Move* list)
{
    assert(piece != PAWN && piece != KING);

    Bitboard bb = piece == KNIGHT ? KNIGHT_MASK[from]
                                  : slider_attack<piece>(from, pos.pieces());
    bb &= target;

    FOR_EACH_BIT(bb, *list++ = create_move(from, sq))

    return list;
}

Move* generate_king_moves(Square from, Bitboard not_allowed, Move* list)
{
    Bitboard bb = KING_MASK[from] & ~not_allowed;
    FOR_EACH_BIT(bb, *list++ = create_move(from, sq));
    return list;
}

template <Color side, Ray ray>
Pin* generate_pin_in_ray(const Position& position, Pin* list,
                         Bitboard* pinned_bb, Bitboard blockers)
{
    const Piece KING_PIECE = make_piece(side, KING);
    Square king_sq = position.piece_position(KING_PIECE, 0);
    Bitboard masked_ray = RAYS[ray][king_sq] & blockers;

    if (popcount_more_than_one(masked_ray))
    {
        Square pinned_sq, attacking_sq;
        if (ray < 4)
        {
            pinned_sq = Square(pop_lsb(&masked_ray));
            attacking_sq = Square(lsb(masked_ray));
        }
        else
        {
            pinned_sq = Square(msb(masked_ray));
            masked_ray &= ~square_bb(pinned_sq);
            attacking_sq = Square(msb(masked_ray));
        }

        if (square_bb(pinned_sq) & position.pieces(side))
        {
            Bitboard sliders = position.pieces(!side, QUEEN);
            if (ray & 1)
                sliders |= position.pieces(!side, ROOK);
            else
                sliders |= position.pieces(!side, BISHOP);

            if (square_bb(attacking_sq) & sliders)
            {
                *list++ = create_pin(
                    pinned_sq, make_piece_kind(position.piece_at(pinned_sq)),
                    ray);
                *pinned_bb |= square_bb(pinned_sq);
            }
        }
    }

    return list;
}

template <Color side>
Pin* generate_pins(const Position& pos, Pin* list, Bitboard* pinned_bb)
{
    Bitboard blockers = pos.pieces();

    list = generate_pin_in_ray<side, RAY_NW>(pos, list, pinned_bb, blockers);
    list = generate_pin_in_ray<side, RAY_N>(pos, list, pinned_bb, blockers);
    list = generate_pin_in_ray<side, RAY_NE>(pos, list, pinned_bb, blockers);
    list = generate_pin_in_ray<side, RAY_E>(pos, list, pinned_bb, blockers);
    list = generate_pin_in_ray<side, RAY_SE>(pos, list, pinned_bb, blockers);
    list = generate_pin_in_ray<side, RAY_S>(pos, list, pinned_bb, blockers);
    list = generate_pin_in_ray<side, RAY_SW>(pos, list, pinned_bb, blockers);
    list = generate_pin_in_ray<side, RAY_W>(pos, list, pinned_bb, blockers);

    return list;
}

template <Color side>
Move* generate_pinned_pawn_moves(Square from, Ray ray, const Position& pos,
                                 Bitboard /* target */, Move* list)
{
    const Direction UP = side == WHITE ? NORTH : SOUTH;
    const Direction DOUBLEUP = Direction(UP + UP);
    const Direction UPRIGHT = side == WHITE ? NORTHEAST : SOUTHWEST;
    const Direction UPLEFT = side == WHITE ? NORTHWEST : SOUTHEAST;
    const Rank rank7 = side == WHITE ? RANK_7 : RANK_2;
    const Bitboard rank2_bb =
        side == WHITE ? RANKS_BB[RANK_2] : RANKS_BB[RANK_7];

    if (rank(from) == rank7)
    {
        switch (ray & 3)
        {
        case 0:
            if (shift<UPLEFT>(square_bb(from)) & pos.pieces(!side))
            {
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPLEFT)), QUEEN);
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPLEFT)), ROOK);
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPLEFT)), KNIGHT);
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPLEFT)), BISHOP);
            }
            break;
        case 1:
            if (shift<UP>(square_bb(from)) & ~pos.pieces())
            {
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UP)), QUEEN);
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UP)), ROOK);
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UP)), KNIGHT);
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UP)), BISHOP);
            }
            break;
        case 2:
            if (shift<UPRIGHT>(square_bb(from)) & pos.pieces(!side))
            {
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPRIGHT)), QUEEN);
                *list++ = create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPRIGHT)), ROOK);
                *list++ =
                    create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPRIGHT)), KNIGHT);
                *list++ =
                    create_promotion(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPRIGHT)), BISHOP);
            }
            break;
        }
    }
    else
    {
        switch (ray & 3)
        {
        case 0:
            if (shift<UPLEFT>(square_bb(from)) & pos.pieces(!side))
                *list++ = create_move(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPLEFT)));
            break;
        case 1:
            if (shift<UP>(square_bb(from)) & ~pos.pieces())
            {
                *list++ = create_move(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UP)));
                if (shift<DOUBLEUP>(square_bb(from) & rank2_bb) & ~pos.pieces())
                    *list++ = create_move(from, Square(static_cast<uint64_t>(from) + 2 * static_cast<uint64_t>(UP)));
            }
            break;
        case 2:
            if (shift<UPRIGHT>(square_bb(from)) & pos.pieces(!side))
                *list++ = create_move(from, Square(static_cast<uint64_t>(from) + static_cast<uint64_t>(UPRIGHT)));
            break;
        }
    }

    return list;
}

template <Color side>
Move* generate_pinned_piece_moves(Pin pin, const Position& pos, Bitboard target,
                                  Move* list)
{
    Square from = pin_square(pin);
    PieceKind piece = pin_piece_kind(pin);
    Ray ray = pin_ray(pin);

    assert(piece != KING);

    // pinned knight cannot move
    if (piece == KNIGHT) return list;

    if (piece == PAWN)
        return generate_pinned_pawn_moves<side>(from, ray, pos, target, list);

    if (!allowed_ray(piece, ray)) return list;

    Bitboard bb = attack_in_line(from, ray, pos.pieces()) & target;
    /* print_bb(bb); */

    FOR_EACH_BIT(bb, *list++ = create_move(from, sq))

    return list;
}

template <Color side>
Move* generate_legal_moves(const Position& pos, Move* list)
{
    const Piece C_KING = side == WHITE ? W_KING : B_KING;
    Bitboard checkers_bb = checkers<side>(pos);

    Bitboard push_mask;
    Bitboard capture_mask;
    Bitboard attacked = forbidden_squares<side>(pos);

    Square king_sq = pos.piece_position(C_KING, 0);

    if (checkers_bb)
    {
        if (popcount_more_than_one(checkers_bb))
            return generate_king_moves(king_sq, attacked | pos.pieces(side),
                                       list);

        capture_mask = checkers_bb;

        Square checker_square = Square(lsb(checkers_bb));
        if (is_piece_slider(pos.piece_at(checker_square)))
            push_mask = LINES[king_sq][checker_square] ^ square_bb(king_sq) ^
                        square_bb(checker_square);
        else
            push_mask = 0ULL;
    }
    else
    {
        push_mask = ~pos.pieces();
        capture_mask = pos.pieces(!side);
    }

    Bitboard pinned = 0ULL;
    Pin* pins_start = PINS;
    Pin* pins_end = generate_pins<side>(pos, pins_start, &pinned);

    Bitboard not_pinned_pawns = pos.pieces(side, PAWN) & ~pinned;
    list = generate_pawn_moves<side>(not_pinned_pawns, ~pos.pieces(), push_mask,
                                     capture_mask, list);

    Bitboard target = capture_mask | push_mask;

    Bitboard not_pinned_knights = pos.pieces(side, KNIGHT) & ~pinned;
    FOR_EACH_BIT(not_pinned_knights,
                 list = generate_piece_moves<KNIGHT>(sq, pos, target, list));

    Bitboard not_pinned_bishops = pos.pieces(side, BISHOP) & ~pinned;
    FOR_EACH_BIT(not_pinned_bishops,
                 list = generate_piece_moves<BISHOP>(sq, pos, target, list));

    Bitboard not_pinned_rooks = pos.pieces(side, ROOK) & ~pinned;
    FOR_EACH_BIT(not_pinned_rooks,
                 list = generate_piece_moves<ROOK>(sq, pos, target, list));

    Bitboard not_pinned_queens = pos.pieces(side, QUEEN) & ~pinned;
    FOR_EACH_BIT(not_pinned_queens,
                 list = generate_piece_moves<QUEEN>(sq, pos, target, list));

    if (pos.enpassant_square() != NO_SQUARE)
        list = generate_enpassant<side>(pos, not_pinned_pawns, push_mask,
                                        capture_mask, pos.enpassant_square(),
                                        list);

    list = generate_king_moves(king_sq, attacked | pos.pieces(side), list);

    if (!checkers_bb)
    {
        for (Pin* iter = pins_start; iter != pins_end; ++iter)
            list = generate_pinned_piece_moves<side>(
                *iter, pos, push_mask | capture_mask, list);

        Bitboard taken_for_castling = attacked | pos.pieces();

        if (side == WHITE)
        {
            if ((pos.castling_rights() & W_OO) &&
                !(taken_for_castling & CASTLING_PATHS[W_OO]))
                *list++ = create_castling(KING_CASTLING);
        }
        else
        {
            if ((pos.castling_rights() & B_OO) &&
                !(taken_for_castling & CASTLING_PATHS[B_OO]))
                *list++ = create_castling(KING_CASTLING);
        }

        if (side == WHITE)
        {
            if ((pos.castling_rights() & W_OOO) &&
                !(taken_for_castling & CASTLING_PATHS[W_OOO]) &&
                !(QUEEN_CASTLING_BLOCK[WHITE] & pos.pieces()))
                *list++ = create_castling(QUEEN_CASTLING);
        }
        else
        {
            if ((pos.castling_rights() & B_OOO) &&
                !(taken_for_castling & CASTLING_PATHS[B_OOO]) &&
                !(QUEEN_CASTLING_BLOCK[BLACK] & pos.pieces()))
                *list++ = create_castling(QUEEN_CASTLING);
        }
    }

    return list;
}

template <Color side>
Move* generate_quiescence(const Position& pos, Move* list)
{
    Bitboard checkers_bb = checkers<side>(pos);

    Bitboard pinned = 0ULL;
    Pin* pins_start = PINS;
    Pin* pins_end = generate_pins<side>(pos, pins_start, &pinned);

    Bitboard capture_mask = 0ULL;

    if (checkers_bb)
    {
        if (popcount_more_than_one(checkers_bb)) return list;

        capture_mask = checkers_bb;
    }
    else
    {
        capture_mask = pos.pieces(!side) & ~pos.pieces(!side, PAWN);
    }

    Bitboard not_pinned_pawns = pos.pieces(side, PAWN) & ~pinned;
    list = generate_pawn_moves<side>(not_pinned_pawns, ~pos.pieces(), 0ULL,
                                     capture_mask, list);

    Bitboard not_pinned_knights = pos.pieces(side, KNIGHT) & ~pinned;
    FOR_EACH_BIT(not_pinned_knights, list = generate_piece_moves<KNIGHT>(
                                         sq, pos, capture_mask, list));

    Bitboard not_pinned_bishops = pos.pieces(side, BISHOP) & ~pinned;
    FOR_EACH_BIT(not_pinned_bishops, list = generate_piece_moves<BISHOP>(
                                         sq, pos, capture_mask, list));

    Bitboard not_pinned_rooks = pos.pieces(side, ROOK) & ~pinned;
    FOR_EACH_BIT(not_pinned_rooks, list = generate_piece_moves<ROOK>(
                                       sq, pos, capture_mask, list));

    Bitboard not_pinned_queens = pos.pieces(side, QUEEN) & ~pinned;
    FOR_EACH_BIT(not_pinned_queens, list = generate_piece_moves<QUEEN>(
                                        sq, pos, capture_mask, list));

    if (!checkers_bb)
    {
        for (Pin* iter = pins_start; iter != pins_end; ++iter)
            list = generate_pinned_piece_moves<side>(*iter, pos, capture_mask,
                                                     list);
    }

    return list;
}

Move* generate_moves(const Position& position, Color side, Move* list)
{
    return side == WHITE ? generate_legal_moves<WHITE>(position, list)
                         : generate_legal_moves<BLACK>(position, list);
}

Move* generate_quiescence_moves(const Position& position, Color side,
                                Move* list)
{
    return side == WHITE ? generate_quiescence<WHITE>(position, list)
                         : generate_quiescence<BLACK>(position, list);
}

Bitboard attacked_squares(const Position& position, Color side)
{
    const Color opponent = !side;
    const Direction UPLEFT = side == BLACK ? NORTHWEST : SOUTHEAST;
    const Direction UPRIGHT = side == BLACK ? NORTHEAST : SOUTHWEST;

    const Piece OPPONENT_KNIGHT = side == WHITE ? B_KNIGHT : W_KNIGHT;
    const Piece OPPONENT_BISHOP = side == WHITE ? B_BISHOP : W_BISHOP;
    const Piece OPPONENT_ROOK = side == WHITE ? B_ROOK : W_ROOK;
    const Piece OPPONENT_QUEEN = side == WHITE ? B_QUEEN : W_QUEEN;
    const Piece OPPONENT_KING = side == WHITE ? B_KING : W_KING;

    Bitboard bb = 0ULL;

    Bitboard pawns = position.pieces(opponent, PAWN);
    bb |= shift(pawns, UPLEFT) | shift(pawns, UPRIGHT);

    for (int i = 0; i < position.no_pieces(OPPONENT_KNIGHT); ++i)
        bb |= KNIGHT_MASK[position.piece_position(OPPONENT_KNIGHT, i)];

    Bitboard blockers = position.pieces();
    for (int i = 0; i < position.no_pieces(OPPONENT_BISHOP); ++i)
        bb |= slider_attack<BISHOP>(position.piece_position(OPPONENT_BISHOP, i),
                                    blockers);
    for (int i = 0; i < position.no_pieces(OPPONENT_ROOK); ++i)
        bb |= slider_attack<ROOK>(position.piece_position(OPPONENT_ROOK, i),
                                  blockers);
    for (int i = 0; i < position.no_pieces(OPPONENT_QUEEN); ++i)
        bb |= slider_attack<QUEEN>(position.piece_position(OPPONENT_QUEEN, i),
                                   blockers);

    bb |= KING_MASK[position.piece_position(OPPONENT_KING, 0)];

    return bb;
}

uint64_t perft(Position& position, int depth)
{
    if (depth == 0) return 1;

    Move* begin = MOVE_LIST[depth];
    Move* end = generate_moves(position, position.color(), begin);

    if (depth == 1) return end - begin;

    uint64_t sum = 0;
    for (Move* it = begin; it != end; ++it)
    {
        Move move = *it;
        MoveInfo moveinfo = position.do_move(move);

        sum += perft(position, depth - 1);

        position.undo_move(move, moveinfo);
    }

    return sum;
}

bool is_move_legal(const Position& position, Move move)
{
    Move* begin = TEMP_MOVE_LIST;
    Move* end = generate_moves(position, position.color(), begin);

    for (Move* it = begin; it != end; ++it)
        if (*it == move) return true;
    return false;
}

};  // namespace engine
