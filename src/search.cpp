#include "move_picker.h"
#include "logger.h"
#include "search.h"
#include "transposition_table.h"

#include <chrono>
#include <cstring>

namespace engine
{

constexpr Score lost_in(int64_t ply)
{
    return -INFINITY_SCORE + ply;
}

constexpr Score win_in(int64_t ply)
{
    return -lost_in(ply);
}

const int64_t INFINITE = 1LL << 32;

Search::Search(const Position& position, const PositionScorer& scorer, const Limits& limits)
    : position(position), scorer(scorer), limits(limits), pv_list(), nodes_searched(0), info()
{
    if (limits.infinite)
    {
        _search_depth = MAX_DEPTH;
        _search_time = INFINITE;
    }
    else if (limits.depth != 0)
    {
        _search_depth = limits.depth;
        _search_time = INFINITE;
    }
    else if (limits.movetime != 0)
    {
        _search_depth = MAX_DEPTH;
        _search_time = limits.movetime;
    }
    else if (limits.timeleft[position.side_to_move()] != 0)
    {
        int our_time = limits.timeleft[position.side_to_move()];
        int movestogo = limits.movestogo == 0 ? 20 : limits.movestogo;

        _search_time = our_time / (movestogo + 1);
        _search_depth = MAX_DEPTH;
    }
    else
    {
        _search_depth = 7;
        _search_time = INFINITE;
    }

}

void Search::stop()
{
    stop_search = true;
}

void Search::go()
{
    Position pos = position;
    stop_search = false;
    start_time = std::chrono::steady_clock::now();

    TimePoint end_time;
    int64_t elapsed = 0LL;

    MoveList temp_pv_list;

    Score min = -INFINITY_SCORE;
    Score max = INFINITY_SCORE;

    _current_depth = 0;
    while (!stop_search)
    {
        _current_depth++;
        nodes_searched = 0;

        Score result = search<true>(pos, _current_depth, min, max, temp_pv_list);

        end_time = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        if (!stop_search)
        {
            pv_list = temp_pv_list;

            std::string score_str;
            if (result < lost_in(MAX_DEPTH))
                score_str = "mate -" + std::to_string(result + INFINITY_SCORE);
            else if (result > win_in(MAX_DEPTH))
                score_str = "mate " + std::to_string(INFINITY_SCORE - result);
            else
                score_str = "cp " + std::to_string(result * 100LL / PIECE_BASE_VALUES[END_GAME][PAWN]);

            logger << "info "
                      << "depth " << _current_depth << " "
                      << "score " << score_str << " "
                      << "nodes " << nodes_searched << " "
                      << "nps " << (nodes_searched * 1000 / (elapsed + 1)) << " "
                      << "time " << elapsed << " "
                      << "pv ";
            Position temp_position = position;
            for (int i = pv_list.size() - 1; i >= 0; --i)
            {
                logger << temp_position.move_to_string(pv_list[i]) << " ";
                temp_position.do_move(pv_list[i]);
            }
            logger << std::endl;
        }

        if (result < lost_in(MAX_DEPTH) || result > win_in(MAX_DEPTH))
            break;

        if (_current_depth >= _search_depth)
            break;

        if (elapsed >= (_search_time / 2))
            break;
    }


    logger << "bestmove " << position.move_to_string(pv_list.back()) << std::endl;
}

template <bool allow_null_move>
Score Search::search(Position& position, int depth, Score alpha, Score beta, MoveList& movelist)
{
    assert(alpha < beta);

    movelist = MoveList();
    if (stop_search || check_limits())
    {
        stop_search = true;
        return 0;
    }

    if (position.threefold_repetition())
        return DRAW_SCORE;
    if (position.rule50())
        return DRAW_SCORE;

    Move* begin = MOVE_LIST[depth];
    Move* end = generate_moves(position, position.side_to_move(), begin);
    bool is_in_check = position.is_in_check(position.side_to_move());

    if (begin == end)
        return is_in_check ? lost_in(_current_depth - depth) : DRAW_SCORE;

    if (depth == 0)
        return quiescence_search(position, MAX_DEPTH - 1, alpha, beta);

    Score best = -INFINITY_SCORE;
    MovePicker movepicker(position, begin, end, info, true);
    MoveList temp_movelist;

    if (allow_null_move)
    {
        Color side = position.side_to_move();
        int n = position.number_of_pieces(make_piece(side, KNIGHT))
              + position.number_of_pieces(make_piece(side, BISHOP))
              + position.number_of_pieces(make_piece(side, ROOK))
              + position.number_of_pieces(make_piece(side, QUEEN));
        if (!is_in_check && n > 0 && depth > 4)
        {
            info.ply++;
            MoveInfo moveinfo = position.do_null_move();
            Score result = -search<false>(position, depth - 4, -beta, -alpha, temp_movelist);
            position.undo_null_move(moveinfo);
            info.ply--;

            if (result >= beta)
                return beta;
        }
    }

    bool search_full_window = true;
    while (movepicker.has_next())
    {
        Move move = movepicker.get_next();
        MoveInfo moveinfo = position.do_move(move);

        info.ply++;
        Score result;
        if (search_full_window)
        {
            result = -search<true>(position, depth - 1, -beta, -alpha, temp_movelist);
        }
        else
        {
            result = -search<true>(position, depth - 1, -alpha - 1, -alpha, temp_movelist);
            if (alpha < result && result < beta)
                result = -search<true>(position, depth - 1, -beta, -alpha, temp_movelist);
        }
        info.ply--;

        position.undo_move(move, moveinfo);

        if (result >= beta)
        {
            if (position.piece_at(to(move)) == NO_PIECE)
            {
                info.update_killers(info.ply, move);
                info.update_history(position.side_to_move(), from(move), to(move), depth);
            }
            movelist = MoveList(temp_movelist);
            movelist.push_back(move);
            info.update_pv(position.hash(), move);
            return beta;
        }

        if (result > best)
        {
            best = result;
            movelist = MoveList(temp_movelist);
            movelist.push_back(move);

        }
        if (result > alpha)
        {
            alpha = result;
            search_full_window = false;
        }
    }

    if (movelist.size() > 0)
        info.update_pv(position.hash(), movelist.back());
    return best;
}

Score Search::quiescence_search(Position& position, int depth, Score alpha, Score beta)
{
    if (stop_search || check_limits())
    {
        stop_search = true;
        return 0;
    }

    if (position.threefold_repetition())
        return DRAW_SCORE;
    if (position.rule50())
        return DRAW_SCORE;

    bool is_in_check = position.is_in_check(position.side_to_move());

    Move* begin = QUIESCENCE_MOVE_LIST[depth];
    Move* end = generate_moves(position, position.side_to_move(), begin);

    if (begin == end)
        return is_in_check ? lost_in(MAX_DEPTH) : DRAW_SCORE;

    end = generate_quiescence_moves(position, position.side_to_move(), begin);

    Score standpat = scorer.score(position);
    nodes_searched++;

    if (depth <= 0)
        return standpat;

    if (standpat >= beta)
        return beta;
    if (standpat > alpha)
        alpha = standpat;


    MovePicker movepicker(position, begin, end, info, false);

    bool search_full_window = true;
    while (movepicker.has_next())
    {
        Move move = movepicker.get_next();
        MoveInfo moveinfo = position.do_move(move);

        Score result;
        if (search_full_window)
            result = -quiescence_search(position, depth - 1, -beta, -alpha);
        else
        {
            result = -quiescence_search(position, depth - 1, -alpha - 1, -alpha);
            if (alpha < result && result < beta)
                result = -quiescence_search(position, depth - 1, -beta, -alpha);
        }
        position.undo_move(move, moveinfo);

        if (result >= beta)
            return beta;
        if (result > alpha)
        {
            alpha = result;
            search_full_window = false;
        }
    }

    return alpha;
}

bool Search::check_limits()
{
    check_limits_counter--;
    if (check_limits_counter > 0)
        return false;

    check_limits_counter = 4096;

    if (limits.nodes > 0 && nodes_searched >= limits.nodes)
    {
        stop_search = true;
        return true;
    }

    TimePoint end_time = std::chrono::steady_clock::now();
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (elapsed >= _search_time)
    {
        stop_search = true;
        return true;
    }

    return false;
}

}
