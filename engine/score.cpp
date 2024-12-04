#include "score.h"

#include "bitboard.h"
#include "bithacks.h"
#include "endgame.h"
#include "logger.h"
#include "position.h"
#include "position_bitboards.h"
#include "types.h"

#include <iomanip>

namespace engine
{
template <Color side>
Rank relative_rank(Rank r)
{
    return side == WHITE ? r : RANK_8 - r;
}

template <Color side>
Square relative_square(Square sq)
{
    return make_square(relative_rank<side>(rank(sq)), file(sq));
}

const Value PIECE_WEIGHTS[PIECE_KIND_NUM] = {0, 0, 1, 1, 2, 4, 0};

const Value MAX_PIECE_WEIGTHS =
    2 * (2 * PIECE_WEIGHTS[KNIGHT] + 2 * PIECE_WEIGHTS[BISHOP] +
         2 * PIECE_WEIGHTS[ROOK] + 1 * PIECE_WEIGHTS[QUEEN]);

PositionScorer::PositionScorer() : _pawn_hash_table(), _weight(-1) {}

void PositionScorer::clear()
{
    _pawn_hash_table.clear();
}

Value PositionScorer::combine(const Score& score)
{
    assert(0 <= _weight && _weight <= MAX_PIECE_WEIGTHS);
    return (score.mg * _weight + score.eg * (MAX_PIECE_WEIGTHS - _weight)) /
           MAX_PIECE_WEIGTHS;
}

Value PositionScorer::score(const Position& position)
{
    Value endgameValue = endgame::score(position);
    if (endgameValue != VALUE_NONE) return endgameValue;

    /* if (!popcount_more_than_one(position.pieces(WHITE))) */
    /*     return endgame::score_endgame<BLACK>(position); */
    /* if (!popcount_more_than_one(position.pieces(BLACK))) */
    /*     return endgame::score_endgame<WHITE>(position); */

    setup<WHITE>(position);
    setup<BLACK>(position);

    _weight = game_phase_weight(position);

    Score pieces = score_pieces(position);
    Score pawns = score_pawns(position);
    Value value = combine(pawns + pieces);

    return position.color() == WHITE ? value : -value;
}

template <Color side>
void PositionScorer::setup(const Position& position)
{
    const Square ownKing = position.piece_position(make_piece(side, KING), 0);

    _attacked_by_bb[side][PAWN] =
        pawn_attacks<side>(position.pieces(make_piece(side, PAWN)));

    _attacked_by_bb[side][KNIGHT] = 0ULL;
    for (int i = 0; i < position.number_of_pieces(make_piece(side, KNIGHT));
         ++i)
    {
        Square sq = position.piece_position(make_piece(side, KNIGHT), i);
        _attacked_by_bb[side][KNIGHT] |= KNIGHT_MASK[sq];
    }

    _attacked_by_bb[side][BISHOP] = 0ULL;
    for (int i = 0; i < position.number_of_pieces(make_piece(side, BISHOP));
         ++i)
    {
        Square sq = position.piece_position(make_piece(side, BISHOP), i);
        // remove own bishops and queens from blockers (x-ray attack)
        Bitboard blockers =
            position.pieces() & (~position.pieces(side, BISHOP, QUEEN));
        _attacked_by_bb[side][BISHOP] |= slider_attack<BISHOP>(sq, blockers);
    }

    _attacked_by_bb[side][ROOK] = 0ULL;
    for (int i = 0; i < position.number_of_pieces(make_piece(side, ROOK)); ++i)
    {
        Square sq = position.piece_position(make_piece(side, ROOK), i);
        // remove own rooks and queens from blockers (x-ray attack)
        Bitboard blockers =
            position.pieces() & (~position.pieces(side, ROOK, QUEEN));
        _attacked_by_bb[side][ROOK] |= slider_attack<ROOK>(sq, blockers);
    }

    _attacked_by_bb[side][QUEEN] = 0ULL;
    for (int i = 0; i < position.number_of_pieces(make_piece(side, QUEEN)); ++i)
    {
        Square sq = position.piece_position(make_piece(side, QUEEN), i);
        // remove own bishops and queens from blockers (x-ray attack)
        Bitboard blockers =
            position.pieces() & (~position.pieces(side, BISHOP, QUEEN));
        _attacked_by_bb[side][QUEEN] |= slider_attack<BISHOP>(sq, blockers);
        // remove own bishops and rooks from blockers (x-ray attack)
        blockers = position.pieces() & (~position.pieces(side, ROOK, QUEEN));
        _attacked_by_bb[side][QUEEN] |= slider_attack<ROOK>(sq, blockers);
    }

    _attacked_by_bb[side][KING] =
        KING_MASK[position.piece_position(make_piece(side, KING), 0)];

    _attacked_by_piece[side] =
        _attacked_by_bb[side][KNIGHT] | _attacked_by_bb[side][BISHOP] |
        _attacked_by_bb[side][ROOK] | _attacked_by_bb[side][QUEEN];

    _outposts_bb[side] = get_outposts<side>(position);

    _blockers_for_king[side] =
        blockers_for_square<side>(position, ownKing, _snipers_for_king[side]);
}

Score PositionScorer::score_pieces(const Position& position)
{
    _side_scores[WHITE] = score_pieces_for_side<WHITE>(position);
    _side_scores[BLACK] = score_pieces_for_side<BLACK>(position);

    return _side_scores[WHITE] - _side_scores[BLACK];
}

template <Color side>
Score PositionScorer::score_pieces_for_side(const Position& position)
{
    const Square ownKing = position.piece_position(make_piece(side, KING), 0);
    const Square opponentsKing =
        position.piece_position(make_piece(!side, KING), 0);

    Score value;

    // knights
    {
        _piece_scores[side][KNIGHT] = Score{};
        constexpr Piece piece = make_piece(side, KNIGHT);
        int no_pieces = position.number_of_pieces(piece);

        for (int i = 0; i < no_pieces; ++i)
        {
            Score score = PIECE_VALUE[KNIGHT];
            Square sq = position.piece_position(piece, i);
            Bitboard attacking = KNIGHT_MASK[sq];

            if (square_bb(sq) & _attacked_by_bb[side][PAWN])
            {
                score += SAFE_KNIGHT;
            }
            score += CONTROL_SPACE[KNIGHT] *
                     Value(popcount(attacking & OPPONENT_RANKS_BB[side]));
            score +=
                CONTROL_CENTER_KNIGHT * Value(popcount(attacking & center_bb));

            score +=
                KING_PROTECTOR_PENALTY[KNIGHT] * Value(distance(ownKing, sq));
            score += KING_ATTACKER_PENALTY[KNIGHT] *
                     Value(distance(opponentsKing, sq));

            Bitboard moves =
                get_real_possible_moves<side>(position, sq, attacking);
            score += MOBILITY_BONUS[KNIGHT] * Value(popcount(moves));

            if (_outposts_bb[side] & square_bb(sq))
            {
                score += OUTPOST_KNIGHT_BONUS;
            }

            _square_scores[sq] =
                std::make_pair(make_piece(side, KNIGHT), score);
            _piece_scores[side][KNIGHT] += score;
        }
        value += _piece_scores[side][KNIGHT];
    }

    // bishop
    {
        _piece_scores[side][BISHOP] = Score{};
        constexpr Piece piece = make_piece(side, BISHOP);
        int no_pieces = position.number_of_pieces(piece);

        for (int i = 0; i < no_pieces; ++i)
        {
            Score score = PIECE_VALUE[BISHOP];
            Square sq = position.piece_position(piece, i);
            Bitboard attacking = slider_attack<BISHOP>(sq, position.pieces());

            Bitboard blockers =
                position.pieces() & ~position.pieces(side, BISHOP, QUEEN);
            Bitboard control_bb = slider_attack<BISHOP>(sq, blockers);
            score += CONTROL_SPACE[BISHOP] *
                     Value(popcount(control_bb & OPPONENT_RANKS_BB[side]));

            Bitboard moves =
                get_real_possible_moves<side>(position, sq, attacking);
            score += MOBILITY_BONUS[BISHOP] * Value(popcount(moves));

            score +=
                KING_PROTECTOR_PENALTY[BISHOP] * Value(distance(ownKing, sq));
            score += KING_ATTACKER_PENALTY[BISHOP] *
                     Value(distance(opponentsKing, sq));

            score += PAWNS_ON_SAME_COLOR_AS_BISHOP_PENALTY *
                     Value(popcount(position.pieces(side, PAWN) &
                                    color_squares_bb[sq_color(sq)]));

            if (_outposts_bb[side] & square_bb(sq))
            {
                score += OUTPOST_BISHOP_BONUS;
            }

            _square_scores[sq] =
                std::make_pair(make_piece(side, BISHOP), score);
            _piece_scores[side][BISHOP] += score;
        }
        if ((position.pieces(side, BISHOP) & white_squares_bb) &&
            (position.pieces(side, BISHOP) & black_squares_bb))
            _piece_scores[side][BISHOP] += BISHOP_PAIR_BONUS;

        value += _piece_scores[side][BISHOP];
    }

    // rook
    {
        _piece_scores[side][ROOK] = Score{};
        constexpr Piece piece = make_piece(side, ROOK);
        int no_pieces = position.number_of_pieces(piece);

        for (int i = 0; i < no_pieces; ++i)
        {
            Score score = PIECE_VALUE[ROOK];
            Square sq = position.piece_position(piece, i);

            Bitboard blockers =
                position.pieces() & ~position.pieces(side, ROOK, QUEEN);
            Bitboard control_bb = slider_attack<ROOK>(sq, blockers);
            score += CONTROL_SPACE[ROOK] *
                     Value(popcount(control_bb & OPPONENT_RANKS_BB[side]));

            Bitboard file_bb = FILES_BB[file(sq)];
            Bitboard rank_bb = RANKS_BB[rank(sq)];
            if (!(file_bb & position.pieces(PAWN)))
                score += ROOK_OPEN_FILE_BONUS;
            if (!(file_bb & position.pieces(side, PAWN)) &&
                file_bb & position.pieces(!side, PAWN))
                score += ROOK_SEMIOPEN_FILE_BONUS;

            if (popcount_more_than_one(file_bb & position.pieces(piece)) ||
                popcount_more_than_one(rank_bb & position.pieces(piece)))
                score += CONNECTED_ROOKS_BONUS / 2;

            Bitboard attacking = slider_attack<ROOK>(sq, position.pieces());

            Bitboard moves =
                get_real_possible_moves<side>(position, sq, attacking);
            score += MOBILITY_BONUS[ROOK] * Value(popcount(moves));

            if (popcount(moves) <= 3)
            {
                if ((file(ownKing) < FILE_E) == (file(sq) < file(ownKing)))
                {
                    const bool canCastle =
                        position.castling_rights() & CASTLING_RIGHTS[side];
                    score +=
                        TRAPPED_ROOK_PENALTY * Value(1 + Value(!canCastle));
                }
            }

            _square_scores[sq] = std::make_pair(make_piece(side, ROOK), score);
            _piece_scores[side][ROOK] += score;
        }

        value += _piece_scores[side][ROOK];
    }

    // queen
    {
        _piece_scores[side][QUEEN] = Score{};
        constexpr Piece piece = make_piece(side, QUEEN);
        int no_pieces = position.number_of_pieces(piece);

        for (int i = 0; i < no_pieces; ++i)
        {
            Score score = PIECE_VALUE[QUEEN];
            Square sq = position.piece_position(piece, i);

            Bitboard blockers =
                position.pieces() & ~position.pieces(side, BISHOP, QUEEN);
            Bitboard control_bb = slider_attack<BISHOP>(sq, blockers);
            score += CONTROL_SPACE[QUEEN] *
                     Value(popcount(control_bb & OPPONENT_RANKS_BB[side]));

            blockers = position.pieces() & ~position.pieces(side, ROOK, QUEEN);
            control_bb = slider_attack<ROOK>(sq, blockers);
            score += CONTROL_SPACE[QUEEN] *
                     Value(popcount(control_bb & OPPONENT_RANKS_BB[side]));

            Bitboard snipers =
                (pseudoattacks<BISHOP>(sq) & position.pieces(!side, BISHOP)) |
                (pseudoattacks<ROOK>(sq) & position.pieces(!side, ROOK));
            Bitboard rest = position.pieces() & ~(snipers | square_bb(sq));

            while (snipers)
            {
                Square sniper_sq = Square(pop_lsb(&snipers));
                Bitboard blockers = LINES[sq][sniper_sq] & rest;
                // check if there is only one blocker between the queen and the
                // sniper
                if (blockers && !popcount_more_than_one(blockers))
                {
                    score += VULNERABLE_QUEEN_PENALTY;
                    break;
                }
            }

            Bitboard attacking = slider_attack<QUEEN>(sq, position.pieces());
            Bitboard moves =
                get_real_possible_moves<side>(position, sq, attacking);
            score += MOBILITY_BONUS[QUEEN] * Value(popcount(moves));

            _square_scores[sq] = std::make_pair(make_piece(side, QUEEN), score);
            _piece_scores[side][QUEEN] += score;
        }

        value += _piece_scores[side][QUEEN];
    }

    _piece_scores[side][KING] = score_king<side>(position);
    _square_scores[ownKing] =
        std::make_pair(make_piece(side, KING), _piece_scores[side][KING]);
    value += _piece_scores[side][KING];

    return value;
}

template <Color side>
Score PositionScorer::score_king_shelter(const Position& position,
                                         Square king_sq)
{
    Bitboard pawns_in_king_area =
        KING_MASK[king_sq] & position.pieces(side, PAWN);
    return Value(popcount(pawns_in_king_area)) * KING_SAFETY_BONUS;
}

template <Color side>
Score PositionScorer::score_king_safety(const Position& position)
{
    const Square ownKing = position.piece_position(make_piece(side, KING), 0);
    position.piece_position(make_piece(!side, KING), 0);
    const Castling kingCastling = side == WHITE ? W_OO : B_OO;
    const Castling queenCastling = side == WHITE ? W_OOO : B_OOO;

    auto compareScores = [](Score a, Score b) { return a.mg < b.mg; };

    Score score = score_king_shelter<side>(position, ownKing);

    // if we can castle kingside check if there is better shelter
    if (position.castling_rights() & kingCastling)
    {
        score = std::max(
            score,
            score_king_shelter<side>(position, relative_square<side>(SQ_G1)),
            compareScores);
    }

    // if we can castle queenside check if there is better shelter (both c1 and
    // b1)
    if (position.castling_rights() & queenCastling)
    {
        score = std::max(
            score,
            score_king_shelter<side>(position, relative_square<side>(SQ_C1)),
            compareScores);
        score = std::max(
            score,
            score_king_shelter<side>(position, relative_square<side>(SQ_B1)),
            compareScores);
    }

    // penalty for distance from own pawns in endgame
    // tries to bring king closer to pawns
    const Piece MY_PAWN = make_piece(side, PAWN);
    int no_pawns = position.number_of_pieces(MY_PAWN);
    for (int i = 0; i < no_pawns; ++i)
    {
        score += Value(distance(ownKing, position.piece_position(MY_PAWN, i))) *
                 KING_PAWN_PROXIMITY_PENALTY;
    }

    return score;
}

template <Color side>
Score PositionScorer::score_king(const Position& position)
{
    const Square ownKing = position.piece_position(make_piece(side, KING), 0);
    const Square opponentsKing =
        position.piece_position(make_piece(!side, KING), 0);

    const Rank first_rank = relative_rank<side>(RANK_1);
    const Rank second_rank = relative_rank<side>(RANK_2);

    Score value;

    Bitboard king_area = KING_MASK[ownKing] | square_bb(ownKing);
    Bitboard moves = KING_MASK[ownKing] & ~attacked_squares(position, !side);

    value += score_king_safety<side>(position);
    value += MOBILITY_BONUS[KING] * Value(popcount(moves));

    if (rank(ownKing) == first_rank && position.pieces(!side, ROOK, QUEEN))
    {
        // check for backrank weakness
        Bitboard backrank_area = king_area & RANKS_BB[second_rank];
        Bitboard blocked = position.pieces(side) | _attacked_by_piece[!side] |
                           _attacked_by_bb[!side][PAWN] |
                           KING_MASK[opponentsKing];
        if ((backrank_area & blocked) == backrank_area)
            value += WEAK_BACKRANK_PENALTY;
    }

    // centralize king in the endgame
    int distanceToCenter = std::min(std::min(distance(ownKing, SQ_D4), distance(ownKing, SQ_D5)),
                                    std::min(distance(ownKing, SQ_E4), distance(ownKing, SQ_E5)));
    value += Value(distanceToCenter) * NOT_CENTRALIZED_KING_PENALTY;

    Bitboard possible_bishop_check = slider_attack<BISHOP>(
        ownKing, position.pieces() & ~_blockers_for_king[side]);
    if (position.pieces(!side, QUEEN) ||
        (position.pieces(!side, BISHOP) & color_squares_bb[sq_color(ownKing)]))
        value += WEAK_KING_DIAGONALS * Value(popcount(possible_bishop_check));
    Bitboard possible_rook_check = slider_attack<ROOK>(
        ownKing, position.pieces() & ~_blockers_for_king[side]);
    if (position.pieces(!side, QUEEN) || position.pieces(!side, ROOK))
        value += WEAK_KING_LINES * Value(popcount(possible_rook_check));

    return value;
};

Score PositionScorer::score_pawns(const Position& position)
{
    uint64_t key = position.pawn_hash();
    bool found = false;
    auto entry = _pawn_hash_table.probe(key, found);

    if (found)
    {
        return entry->value;
    }

    Score score = score_pawns_for_side<WHITE>(position) -
                  score_pawns_for_side<BLACK>(position);
    _pawn_hash_table.insert(key, score);
    return score;
}

template <Color side>
Score PositionScorer::score_pawns_for_side(const Position& position)
{
    constexpr Piece pawn = make_piece(side, PAWN);
    constexpr Direction up_dir = side == WHITE ? NORTH : SOUTH;
    constexpr Direction down_dir = static_cast<Direction>(-up_dir);
    constexpr int up = side == WHITE ? 1 : -1;

    Score value{};
    int num_pawns = position.number_of_pieces(pawn);

    Bitboard ourPawns = position.pieces(side, PAWN);
    Bitboard theirPawns = position.pieces(!side, PAWN);

    for (int i = 0; i < num_pawns; ++i)
    {
        Score score = PIECE_VALUE[PAWN];
        Square sq = position.piece_position(pawn, i);

        Rank r = rank(sq);
        File f = file(sq);
        Rank rel_rank = relative_rank<side>(r);

        const Bitboard attacks = pawn_attacks<side>(square_bb(sq));
        const Bitboard neighbours = ourPawns & NEIGHBOUR_FILES_BB[f];
        const Bitboard phalanx = neighbours & RANKS_BB[r];
        const Bitboard support = neighbours & RANKS_BB[r - up];
        const Bitboard lever = theirPawns & attacks;
        const Bitboard leverPush = theirPawns & shift<up_dir>(attacks);
        const Bitboard opposed = theirPawns & passed_pawn_bb<side>(sq);

        const bool blocked = theirPawns & shift<up_dir>(square_bb(sq));
        const bool doubled = ourPawns & shift<down_dir>(square_bb(sq));
        const bool backward =
            !(neighbours & passed_pawn_bb<!side>(Square(sq + 8 * up))) &&
            (blocked || leverPush);
        const bool passed = !opposed || !(opposed ^ lever) ||
                            (!(opposed ^ leverPush) &&
                             popcount(phalanx) >= popcount(leverPush));

        score += popcount(attacks & opponents_center_bb[side]) *
                 PAWN_CONTROL_CENTER_BONUS;
        if (doubled) score += DOUBLE_PAWN_PENALTY;
        if (support | phalanx)
        {
            score += Score(CONNECTED_PAWNS_BONUS[rel_rank] *
                           (1 + static_cast<int>(bool(phalanx)) -
                            static_cast<int>(bool(opposed))));
            score += Score(10 * popcount(support));
        }
        else if (!neighbours)
            score += ISOLATED_PAWN_PENALTY;
        else if (backward)
            score += BACKWARD_PAWN_PENALTY;

        if (passed)
            score += PASSED_PAWN_BONUS * PASSED_PAWN_RANK_WEIGHT[rel_rank];

        _square_scores[sq] = std::make_pair(pawn, score);
        value += score;
    }

    _piece_scores[side][PAWN] = value;

    return value;
}

Value PositionScorer::game_phase_weight(const Position& position)
{
    Value weight = 0;
    weight += position.number_of_pieces(W_KNIGHT) * PIECE_WEIGHTS[KNIGHT];
    weight += position.number_of_pieces(W_BISHOP) * PIECE_WEIGHTS[BISHOP];
    weight += position.number_of_pieces(W_ROOK) * PIECE_WEIGHTS[ROOK];
    weight += position.number_of_pieces(W_QUEEN) * PIECE_WEIGHTS[QUEEN];
    weight += position.number_of_pieces(B_KNIGHT) * PIECE_WEIGHTS[KNIGHT];
    weight += position.number_of_pieces(B_BISHOP) * PIECE_WEIGHTS[BISHOP];
    weight += position.number_of_pieces(B_ROOK) * PIECE_WEIGHTS[ROOK];
    weight += position.number_of_pieces(B_QUEEN) * PIECE_WEIGHTS[QUEEN];

    return std::min(weight, MAX_PIECE_WEIGTHS);
}

template <Color side>
Bitboard PositionScorer::blockers_for_square(const Position& position,
                                             Square sq, Bitboard& snipers)
{
    snipers = no_squares_bb;
    Bitboard blockers = no_squares_bb;

    Bitboard possible_snipers =
        (pseudoattacks<BISHOP>(sq) & position.pieces(!side, BISHOP, QUEEN)) |
        (pseudoattacks<ROOK>(sq) & position.pieces(!side, ROOK, QUEEN));

    Bitboard rest = position.pieces() & ~(possible_snipers | square_bb(sq));

    while (possible_snipers)
    {
        Square sniper_sq = Square(pop_lsb(&possible_snipers));

        Bitboard b = LINES[sq][sniper_sq] & rest;
        if (b && !popcount_more_than_one(b))
        {
            snipers |= square_bb(sq);
            blockers |= b;
        }
    }

    return blockers;
}

template <Color side>
Bitboard PositionScorer::get_real_possible_moves(const Position& position,
                                                 Square sq, Bitboard moves)
{
    const Square ownKing = position.piece_position(make_piece(side, KING), 0);

    Bitboard opponent_pieces =
        position.pieces(!side) & ~position.pieces(!side, PAWN);

    if (square_bb(sq) & _blockers_for_king[side])
    {
        moves &= FULL_LINES[sq][ownKing];
    }

    // cannot move into square with our piece
    moves &= ~position.pieces(side);

    // don't count squares attacked by opponents pawns with nothing valueable
    // there
    moves &= ~(_attacked_by_bb[!side][PAWN] & ~opponent_pieces);

    // don't count squares attacked by opponents piece and not defended by our
    // pawns
    /* moves &= */
    /*     ~(_attacked_by_piece[!side] & ~_attacked_by_bb[side][PAWN]); */

    return moves;
}

void PositionScorer::print_stats()
{
#define TERM(white, black) \
    white << "|" << black << "|" << (white - black) << "|"

    Score total_white = _side_scores[WHITE] + _piece_scores[WHITE][PAWN];
    Score total_black = _side_scores[BLACK] + _piece_scores[BLACK][PAWN];

    const std::string pieceChar = " PNBRQKpnbrqk";
    const std::string colorReset = "\033[m";
    const std::string colorBold = "\033[1m";
    const std::string sideColor[COLOR_NUM] = {colorBold, colorReset};
    const std::string pieceKindColor[PIECE_KIND_NUM] = {
        "",
        "\033[31m",  // pawn
        "\033[32m",  // knight
        "\033[33m",  // bishop
        "\033[34m",  // rook
        "\033[35m",  // queen
        "\033[36m",  // king
    };

    const std::string horizontalLine =
        "+-----+-----+-----+-----+-----+-----+-----+-----+";

    for (Rank rank = RANK_8; rank >= RANK_1; --rank)
    {
        logger << horizontalLine << std::endl;
        for (File file = FILE_A; file <= FILE_H; ++file)
        {
            Piece piece = _square_scores[make_square(rank, file)].first;
            logger << "|  " << sideColor[get_color(piece)]
                      << pieceKindColor[get_piece_kind(piece)]
                      << pieceChar[piece] << "  " << colorReset;
        }
        logger << "|" << std::endl;
        for (File file = FILE_A; file <= FILE_H; ++file)
        {
            const auto [piece, score] = _square_scores[make_square(rank, file)];
            logger << "|" << sideColor[get_color(piece)]
                      << pieceKindColor[get_piece_kind(piece)] << std::setw(5)
                      << std::setfill(' ')
                      << (piece != NO_PIECE ? std::to_string(combine(score))
                                            : "")
                      << colorReset;
        }
        logger << "|" << std::endl;
    }
    logger << horizontalLine << std::endl;

    logger << std::showpoint << std::noshowpos << std::fixed
              << std::setprecision(2)
              << "+---------+--------------+--------------+--------------+"
              << std::endl
              << "|  TYPE   |     White    |     Black    |     Total    |"
              << std::endl
              << "|         |   Mg    Eg   |   Mg    Eg   |   Mg    Eg   |"
              << std::endl
              << "+---------+--------------+--------------+--------------+"
              << std::endl
              << "|   PAWNS |"
              << TERM(_piece_scores[WHITE][PAWN], _piece_scores[BLACK][PAWN])
              << std::endl
              << "| KNIGHTS |"
              << TERM(_piece_scores[WHITE][KNIGHT],
                      _piece_scores[BLACK][KNIGHT])
              << std::endl
              << "| BISHOPS |"
              << TERM(_piece_scores[WHITE][BISHOP],
                      _piece_scores[BLACK][BISHOP])
              << std::endl
              << "|   ROOKS |"
              << TERM(_piece_scores[WHITE][ROOK], _piece_scores[BLACK][ROOK])
              << std::endl
              << "|  QUEENS |"
              << TERM(_piece_scores[WHITE][QUEEN], _piece_scores[BLACK][QUEEN])
              << std::endl
              << "|    KING |"
              << TERM(_piece_scores[WHITE][KING], _piece_scores[BLACK][KING])
              << std::endl
              << "|   TOTAL |" << TERM(total_white, total_black) << std::endl
              << "|  WEIGHT |" << Score(_weight, MAX_PIECE_WEIGTHS)
              << "|              |              |" << std::endl
              << "+---------+--------------+--------------+--------------+"
              << std::endl;
}

}  // namespace engine
