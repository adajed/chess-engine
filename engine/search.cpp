#include "search.h"

#include "endgame.h"
#include "logger.h"
#include "movegen.h"
#include "score.h"
#include "search_utils.h"
#include "time_manager.h"
#include "transposition_table.h"
#include "types.h"
#include "utils.h"
#include "value.h"

#include "syzygy/tbprobe.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace engine
{

int late_move_reduction(Depth /* depth */, int move_number)
{
    move_number = std::min(move_number, 64);
    return static_cast<int>(std::floor(1 + std::log(move_number)));
}

std::string score2str(Value score)
{
    if (score <= lost_in(MAX_DEPTH))
        return "mate -" + std::to_string(VALUE_MATE + score);
    else if (score >= win_in(MAX_DEPTH))
        return "mate " + std::to_string(VALUE_MATE - score);
    else
        return "cp " + std::to_string(score * 100LL / PIECE_VALUE[PAWN].eg);
}

void clear_pv_list(Info* info)
{
    ASSERT(info != nullptr);
    info->_pv_list_length = 0;
}

void set_new_pv_list(Info* info, Move move)
{
    ASSERT(info != nullptr);
    info->_pv_list_length = 1;
    info->_pv_list[0] = move;
}

void add_new_move_to_pv_list(Info* destInfo, Move move, Info* srcInfo)
{
    ASSERT(srcInfo != nullptr);
    ASSERT(destInfo != nullptr);
    const int len = srcInfo->_pv_list_length;
    ASSERT(len < MAX_DEPTH * 2);
    destInfo->_pv_list_length = len + 1;
    destInfo->_pv_list[0] = move;
    std::memcpy(destInfo->_pv_list.data() + 1, srcInfo->_pv_list.data(),
                len * sizeof(Move));
}

void add_bonus(int* v, int b)
{
    int x = *v;
    *v = std::min(x + b, 20000);
}

void update_move_scores(const Position& position, Move move, Info* info,
                        HistoryScore& historyScore, Depth depth)
{
    Square from_sq = from(move);
    Square to_sq = to(move);
    Piece moved_piece = position.piece_at(from_sq);

    if (move != info->_killer_moves[0])
    {
        info->_killer_moves[1] = info->_killer_moves[0];
        info->_killer_moves[0] = move;
    }

    int bonus = depth * depth;

    add_bonus(&historyScore[position.color()][from_sq][to_sq], bonus);
    if ((info - 1)->_current_move != NO_MOVE)
        add_bonus(&(*(info - 1)->_counter_move)[moved_piece][to_sq], bonus);
}

Value compute_search_delta(Move* previous_best_moves, Depth current_depth,
                           Value /* current_score */)
{
    int no_times_move_repeated = 0;
    for (Depth i = current_depth - 1; i > 0; i--)
    {
        if (previous_best_moves[i] == previous_best_moves[i - 1])
        {
            no_times_move_repeated++;
        }
        else
        {
            break;
        }
    }

    double defaultDelta = PIECE_VALUE[PAWN].eg / 4;
    double delta = defaultDelta / (0.1 * std::pow(no_times_move_repeated, 1.25) + 1.0);
    return delta;
}

Search::Search(const Position& position, const Limits& limits,
               PositionScorer& scorer, tt::TTable& ttable)
    : _position(position),
      _scorer(scorer),
      limits(limits),
      check_limits_counter(4096),
      stop_search(false),
      _search_time(0),
      _search_depth(0),
      _current_depth(0),
      _max_nodes_searched(limits.nodes),
      _best_move(NO_MOVE),
      _start_time(),
      _root_moves(),
      _ttable(ttable),
      _stack_info(),
      _history_score(),
      _move_orderer(_ttable, _history_score),
      _counter_move_table()
{
    if (limits.searchmovesnum > 0)
    {
        const Move* begin = limits.searchmoves;
        const Move* end = limits.searchmoves + limits.searchmovesnum;
        _root_moves.insert(_root_moves.end(), begin, end);
    }
    else
    {
        Move* begin = MOVE_LIST[0];
        Move* end = generate_moves(_position, _position.color(), begin);
        _root_moves.insert(_root_moves.end(), begin, end);
    }

    if (limits.infinite)
    {
        _search_depth = MAX_DEPTH;
        _search_time = INFINITE_DURATION;
    }
    else if (limits.depth != 0)
    {
        _search_depth = limits.depth;
        _search_time = INFINITE_DURATION;
    }
    else if (limits.movetime != 0)
    {
        _search_depth = MAX_DEPTH;
        _search_time = limits.movetime;
    }
    else if (limits.timeleft[position.color()] != 0)
    {
        _search_time = TimeManager::calculateTime(limits, position.color(),
                                                  position.ply_count());
        _search_depth = MAX_DEPTH;
    }
    else
    {
        _search_depth = 7;
        _search_time = INFINITE_DURATION;
    }

    if (_max_nodes_searched == 0) _max_nodes_searched = MAX_NODE_COUNT;
}

void Search::stop()
{
    stop_search = true;
}

void Search::go()
{
    init_search();
    stop_search = false;
    _start_time = std::chrono::steady_clock::now();

    // check if there is only one move to make
    if (_root_moves.size() == 1)
    {
        _search_time = 500;
    }
    iter_search();

    ASSERT(_best_move != NO_MOVE);
    logger_sync_out << "bestmove " << _position.uci(_best_move) << sync_endl;
}

void Search::init_search()
{
    for (Color c : {WHITE, BLACK})
        for (Square sq1 = SQ_A1; sq1 <= SQ_H8; ++sq1)
            for (Square sq2 = SQ_A1; sq2 <= SQ_H8; ++sq2)
                _history_score[c][sq1][sq2] = 0;

    for (Piece p1 = W_PAWN; p1 <= B_KING; ++p1)
    {
        for (Square sq1 = SQ_A1; sq1 <= SQ_H8; ++sq1)
        {
            for (Piece p2 = W_PAWN; p2 <= B_KING; ++p2)
                for (Square sq2 = SQ_A1; sq2 <= SQ_H8; ++sq2)
                    _counter_move_table[p1][sq1][p2][sq2] = 0;
        }
    }
}

void Search::print_info(Value result, Depth depth, int64_t elapsed, Info* info)
{
    logger_sync_out << "info "
              << "depth " << depth << " "
              << "score " << score2str(result) << " "
              << "nodes " << _stats.nodes_searched << " "
#if LOG_LEVEL > 0
              << "pvnodes " << _stats.pv_nodes_searched << " "
              << "nonpvnodes " << _stats.non_pv_nodes_searched << " "
              << "qpvnodes " << _stats.quiescence_pv_nodes_searched << " "
              << "qnonpvnodes " << _stats.quiescence_nonpv_nodes_searched << " "
#endif
              << "nps " << (_stats.nodes_searched * 1000 / (elapsed + 1)) << " "
              << "tbhits " << _stats.tb_hits << " "
              << "hashfull " << _ttable.hashfull() << " "
              << "time " << elapsed << " "
              << "pv ";
    Position temp_position = _position;
    for (int i = 0; i < info->_pv_list_length; ++i)
    {
        Move m = info->_pv_list[i];
        logger << temp_position.uci(m) << " ";
        temp_position.do_move(m);
    }
    logger << sync_endl;
}

void Search::iter_search()
{
    _best_move = NO_MOVE;

    TimePoint end_time;
    int64_t elapsed = 0LL;

    MoveList pv_list;
    Value previous_score = VALUE_DRAW;
    Move previous_moves[MAX_DEPTH + 1];
    previous_moves[0] = NO_MOVE;
    Value min_bound = -VALUE_INFINITE;
    Value max_bound = VALUE_INFINITE;

    Info* info = _stack_info.data();
    info->_ply = -1;
    info->_current_move = NO_MOVE;
    info->_counter_move = &_counter_move_table[NO_PIECE][1];

    _current_depth = 0;
    while (!stop_search)
    {
        _current_depth++;

        _stats = SearchStats{};

        Value result;
        Value delta = compute_search_delta(previous_moves, _current_depth,
                                           previous_score);
        if (_current_depth > 2)
        {
            min_bound = std::max(previous_score - delta, -VALUE_INFINITE);
            max_bound = std::min(previous_score + delta, VALUE_INFINITE);
        }

        while (true)
        {
            ASSERT(min_bound < max_bound);
            LOG_INFO("Search depth=%d bound=[%ld, %ld] delta=%ld",
                      _current_depth, min_bound, max_bound, delta);
            result = search(_position, _current_depth, min_bound, max_bound,
                            info + 1);
            LOG_INFO("Result %ld", result);

            if (result <= min_bound)
            {
                min_bound = std::max(min_bound - delta, -VALUE_INFINITE);
                max_bound = std::min(result + 1, VALUE_INFINITE);
            }
            else if (result >= max_bound)
            {
                // if we fail high, don't change min_bound
                max_bound = std::min(max_bound + delta, VALUE_INFINITE);
            }
            else
                break;

            if (stop_search || check_limits()) break;

            delta = std::min(static_cast<Value>(static_cast<float>(delta) * 1.25f),
                             VALUE_INFINITE);
        }

        previous_score = result;

        end_time = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                      end_time - _start_time)
                      .count();

        if (!stop_search)
        {
            Info* realInfo = info + 1;
            ASSERT(realInfo->_pv_list_length > 0);
            print_info(result, _current_depth, elapsed, realInfo);
            _best_move = realInfo->_pv_list[0];
        }
        previous_moves[_current_depth] = _best_move;

        if (is_mate(result)) break;

        if (_current_depth >= _search_depth) break;

        if (elapsed >= 3 * (_search_time / 4)) break;
    }
}

Value Search::search(Position& position, Depth depth, Value alpha, Value beta,
                     Info* info)
{
    ASSERT(alpha < beta);

    info->_ply = (info - 1)->_ply + 1;
    clear_pv_list(info);

    const bool ROOT_NODE = info->_ply == 0;
    const bool PV_NODE = beta != alpha + 1;
    const bool IS_NULL = (info - 1)->_current_move == NO_MOVE;

    LOG_DEBUG("[%d] ENTER SEARCH depth=%d alpha=%ld beta=%ld pvNode=%d fen=%s",
              info->_ply, depth, alpha, beta, static_cast<int>(PV_NODE), position.fen().c_str());

    if (stop_search || check_limits())
    {
        stop_search = true;
        EXIT_SEARCH(Value(0));
    }

    // cannot check it in ROOT_NODE as it might return
    // without any move
    if (!ROOT_NODE && (position.is_repeated() || position.is_draw())) EXIT_SEARCH(VALUE_DRAW);

    (info + 2)->_killer_moves[0] = (info + 2)->_killer_moves[1] = NO_MOVE;

    Move* begin = ROOT_NODE ? &(*_root_moves.begin()) : MOVE_LIST[info->_ply];
    Move* end = ROOT_NODE ? &(*_root_moves.end())
                          : generate_moves(position, position.color(), begin);
    const int n_moves = end - begin;

    bool is_in_check = position.is_in_check(position.color());
    if (is_in_check) depth++;

    if (n_moves == 0) EXIT_SEARCH(is_in_check ? lost_in(0) : VALUE_DRAW);

    if (depth == 0 || info->_ply >= MAX_DEPTH)
    {
        LOG_DEBUG("[%d] START QUIESCENCE_SEARCH", info->_ply);
        Value result =
            quiescence_search(position, MAX_DEPTH - 1, alpha, beta, info);
        LOG_DEBUG("[%d] END QUIESCENCE_SEARCH", info->_ply);
        EXIT_SEARCH(result);
    }

    // update search stats
    _stats.nodes_searched++;
    (PV_NODE ? _stats.pv_nodes_searched : _stats.non_pv_nodes_searched)++;
#if LOG_LEVEL >= 2
    uint64_t savedNumNodesSearched = _stats.nodes_searched;
#endif

    Value bestValue = -VALUE_INFINITE;
    bool found = false;
    auto entryPtr = _ttable.probe(position.hash(), found);

    // internal iterative deepening
    if (PV_NODE && !found && depth > 5)
    {
        search(position, depth - 2, alpha, beta, info);
        entryPtr = _ttable.probe(position.hash(), found);
    }

    if (found && (entryPtr->value.depth >= depth) &&
        (std::find(begin, end, entryPtr->value.move) != end))
    {
        _stats.tt_hits++;
        LOG_DEBUG("[%d] CACHE HIT score=%ld depth=%d flag=%d move=%s",
                  info->_ply, entryPtr->value.score, entryPtr->value.depth,
                  static_cast<int>(entryPtr->value.flag),
                  position.uci(entryPtr->value.move).c_str());

        const bool allowCutoff =
            !PV_NODE || _ttable.isCurrentEpoch(entryPtr->epoch);

        if (allowCutoff)
        {
            switch (entryPtr->value.flag)
            {
            case tt::Flag::kEXACT:
                set_new_pv_list(info, entryPtr->value.move);
                LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
                EXIT_SEARCH(Value(entryPtr->value.score));
            case tt::Flag::kLOWER_BOUND:
                bestValue = entryPtr->value.score;
                alpha = std::max(alpha, entryPtr->value.score);
                break;
            case tt::Flag::kUPPER_BOUND:
                beta = std::min(beta, entryPtr->value.score);
                break;
            }
        }

        if (alpha >= beta)
        {
            LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
            EXIT_SEARCH(Value(entryPtr->value.score));
        }
    }

    // Step 5. Tablebases probe
    if (!ROOT_NODE && Tablebases::ProbeLimit)
    {
        int piecesCount = popcount(position.pieces());

        if (    piecesCount <= Tablebases::ProbeLimit
            && (piecesCount <  Tablebases::ProbeLimit || depth >= Tablebases::ProbeDepth)
            /* &&  position.half_moves() == 0 */
            &&  position.castling_rights() == NO_CASTLING)
        {
            Tablebases::ProbeState err;
            Tablebases::WDLScore wdl = Tablebases::probe_wdl(position, &err);

            // Force check of time on the next occasion
            /* if (thisThread == Threads.main()) */
            /*     static_cast<MainThread*>(thisThread)->callsCnt = 0; */

            if (err != Tablebases::ProbeState::FAIL)
            {
                _stats.tb_hits++;

                int drawScore = Tablebases::Use50Rule ? 1 : 0;

                // use the range VALUE_MATE_IN_MAX_PLY to VALUE_TB_WIN_IN_MAX_PLY to score
                Value value =  wdl < -drawScore ? lost_in(info->_ply + 1)
                            :  wdl >  drawScore ? win_in(info->_ply - 1)
                            : VALUE_DRAW + 2 * wdl * drawScore;

                tt::Flag b =  wdl < -drawScore ? tt::Flag::kUPPER_BOUND
                         : wdl >  drawScore ? tt::Flag::kLOWER_BOUND : tt::Flag::kEXACT;

                if (    b == tt::Flag::kEXACT
                    || (b == tt::Flag::kLOWER_BOUND ? value >= beta : value <= alpha))
                {
                    tt::TTEntry entry(value, std::min(MAX_PLIES - 1, depth + 6), b,
                                      NO_MOVE);
                    _ttable.insert(position.hash(), entry);

                    return value;
                }

                if (PV_NODE)
                {
                    if (b == tt::Flag::kLOWER_BOUND)
                    {
                        bestValue = value;
                        alpha = std::max(alpha, value);
                    }
                    else
                    {
                        beta = std::min(beta, value);
                    }
                }
            }
        }
    }

    if (is_in_check)
    {
        info->_static_eval = VALUE_NONE;
    }
    else if (found && entryPtr->value.flag == tt::Flag::kEXACT)
    {
        info->_static_eval = entryPtr->value.score;
    }
    else
    {
        info->_static_eval = _scorer.score(position);
    }
    LOG_DEBUG("[%d] POSITION score=%ld", info->_ply, info->_static_eval);

    /* bool improving = true; */
    /* if (is_in_check) */
    /*     improving = false; */
    /* else if (info->_ply >= 2 && (info - 2)->_static_eval != VALUE_NONE) */
    /*     improving = info->_static_eval > (info - 2)->_static_eval; */
    /* else if (info->_ply >= 4 && (info - 4)->_static_eval != VALUE_NONE) */
    /*     improving = info->_static_eval > (info - 4)->_static_eval; */
    /* LOG_DEBUG("[%d] IMPROVING %d", info->_ply, int(improving)); */

    // null move pruning
    if (!PV_NODE && !IS_NULL && !is_in_check &&
        info->_static_eval >= beta &&
        position.no_nonpawns(position.color()) > 0 && depth > 4)
    {
        int reducedDepth = 3 + depth / 4 - (info->_static_eval - beta) / PIECE_VALUE[PAWN].eg;
        reducedDepth = std::max(reducedDepth, 0);

        LOG_DEBUG("[%d] DO MOVE nullmove alpha=%ld beta=%ld", info->_ply, alpha,
                  beta);
        MoveInfo moveinfo = position.do_null_move();
        info->_current_move = NO_MOVE;  // this means that this was a null move
        info->_counter_move = &_counter_move_table[NO_PIECE][0];  // trash
        Value result =
            -search(position, reducedDepth, -beta, -beta + 1, info + 1);
        LOG_DEBUG("[%d] UNDO MOVE nullmove", info->_ply);
        position.undo_null_move(moveinfo);

        if (result >= beta && depth < 14)
        {
            LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
            EXIT_SEARCH(beta);
        }

        if (result >= beta)
        {
            // verification search
            LOG_DEBUG("[%d] DO MOVE nullmove alpha=%ld beta=%ld", info->_ply,
                      alpha, beta);
            result = search(position, reducedDepth, beta - 1, beta, info + 1);
            LOG_DEBUG("[%d] UNDO MOVE nullmove", info->_ply);
            if (result >= beta)
            {
                LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
                EXIT_SEARCH(beta);
            }
        }
    }

    constexpr Value FUTILITY_DEPTH_1_MARGIN = PIECE_VALUE[KNIGHT].eg;
    constexpr Value FUTILITY_DEPTH_2_MARGIN = PIECE_VALUE[ROOK].eg;
    const bool doFutilityPruning =
        !is_in_check && std::abs(alpha) < VALUE_ALL_PIECES &&
        std::abs(beta) < VALUE_ALL_PIECES &&
        std::abs(info->_static_eval) < VALUE_ALL_PIECES &&
        ((depth == 1 && info->_static_eval + FUTILITY_DEPTH_1_MARGIN < alpha) ||
         (depth == 2 && info->_static_eval + FUTILITY_DEPTH_2_MARGIN < alpha));
    if (doFutilityPruning)
    {
        LOG_DEBUG("[%d] FUTILITY PRUNING", info->_ply);
    }

    Move best_move = NO_MOVE;
    _move_orderer.order_moves(position, begin, end, info);

    for (int move_count = 0; move_count < n_moves; ++move_count)
    {
        const Move move = begin[move_count];
        const bool moveIsQuiet = position.move_is_quiet(move);

        if (doFutilityPruning && moveIsQuiet
                && !position.move_gives_check(move))
        {
            continue;
        }

        LOG_DEBUG("[%d] DO MOVE %s alpha=%ld beta=%ld", info->_ply,
                  position.uci(move).c_str(), alpha, beta);
        const MoveInfo moveinfo = position.do_move(move);

        info->_current_move = move;
        info->_counter_move =
            &_counter_move_table[position.piece_at(from(move))][to(move)];

        const PieceKind capturedPiece = captured_piece(moveinfo);
        const PieceKind promotedPiece = promotion(move);

        Value result = VALUE_NONE;
        int reduction = 0;
        const Value updated_score = info->_static_eval
                                  + PIECE_VALUE[capturedPiece].eg
                                  + PIECE_VALUE[promotedPiece].eg;

        if (depth > 3 && (moveIsQuiet || updated_score <= alpha))
        {
            reduction = late_move_reduction(depth, move_count + 1);
            reduction -= 2 * static_cast<int>(PV_NODE);

            if (moveIsQuiet)
            {
                if (move == info->_killer_moves[0] ||
                    move == info->_killer_moves[1])
                    reduction--;
            }
            else
            {
                reduction += std::min(
                    2, static_cast<int>((alpha - updated_score) / 300));
            }
            reduction = std::min(depth - 1, std::max(reduction, 0));
        }

        result = -search(position, depth - 1 - reduction, -(alpha + 1), -alpha,
                         info + 1);

        if (reduction > 0 && result > alpha)
        {
            result =
                -search(position, depth - 1, -(alpha + 1), -alpha, info + 1);
        }
        if (PV_NODE && alpha < result && result < beta)
        {
            result = -search(position, depth - 1, -beta, -alpha, info + 1);
        }

        position.undo_move(move, moveinfo);
        LOG_DEBUG("[%d] UNDO MOVE %s", info->_ply,
                  position.uci(move).c_str());

        if (is_mate(result))
        {
            result += result > VALUE_DRAW ? -1 : 1;
        }

        if (result > bestValue)
        {
            bestValue = result;

            if (result > alpha)
            {
                alpha = result;
                best_move = move;
                if (PV_NODE)
                {
                    add_new_move_to_pv_list(info, move, info + 1);
                }

                if (result >= beta)
                {
                    ASSERT(move != NO_MOVE);
                    if (moveIsQuiet)
                    {
                        update_move_scores(position, move, info, _history_score,
                                           depth);
                    }

                    tt::TTEntry entry(result, depth, tt::Flag::kLOWER_BOUND,
                                      move);
                    _ttable.insert(position.hash(), entry);

#if LOG_LEVEL > 1
                    {
                        Position temp_position = position;
                        std::cerr << "[" << info->_ply << "] PV LIST ";
                        for (int i = 0; i < info->_pv_list_length; ++i)
                        {
                            Move m = info->_pv_list[i];
                            std::cerr << temp_position.uci(m) << " ";
                            temp_position.do_move(m);
                        }
                        std::cerr << "\n";
                    }
#endif

                    LOG_DEBUG("[%d] BEST MOVE %s", info->_ply,
                              position.uci(move).c_str());
                    LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
                    EXIT_SEARCH(result);
                }
            }
        }
    }

    if (best_move == NO_MOVE && PV_NODE)
    {
        best_move = begin[0];
        set_new_pv_list(info, best_move);
    }
    else
    {
        tt::Flag flag = PV_NODE ? tt::Flag::kEXACT : tt::Flag::kUPPER_BOUND;
        tt::TTEntry entry(bestValue, depth, flag, best_move);
        _ttable.insert(position.hash(), entry);

        LOG_DEBUG("[%d] BEST MOVE %s", info->_ply,
                  position.uci(best_move).c_str());
    }

#if LOG_LEVEL > 1
    {
        Position temp_position = position;
        std::cerr << "[" << info->_ply << "] PV LIST ";
        for (int i = 0; i < info->_pv_list_length; ++i)
        {
            Move m = info->_pv_list[i];
            std::cerr << temp_position.uci(m) << " ";
            temp_position.do_move(m);
        }
        std::cerr << "\n";
    }
#endif

    LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
    EXIT_SEARCH(bestValue);
}

Value Search::quiescence_search(Position& position, Depth depth, Value alpha,
                                Value beta, Info* info)
{
    ASSERT(alpha < beta);

    info->_ply = (info - 1)->_ply + 1;
    clear_pv_list(info);

    const bool PV_NODE = beta != alpha + 1;

    LOG_DEBUG("[%d] ENTER QUIESCENCE_SEARCH depth=%d alpha=%ld beta=%ld pvNode=%d fen=%s",
              info->_ply, depth, alpha, beta, static_cast<int>(PV_NODE), position.fen().c_str());

    if (stop_search || check_limits())
    {
        stop_search = true;
        EXIT_QSEARCH(Value(0));
    }

    bool is_in_check = position.is_in_check(position.color());

    if (depth <= 0)
        EXIT_QSEARCH(is_in_check ? VALUE_DRAW : _scorer.score(position));

    if (position.is_draw()) EXIT_QSEARCH(VALUE_DRAW);

    // update search stats
    _stats.nodes_searched++;
    (PV_NODE ? _stats.quiescence_pv_nodes_searched : _stats.quiescence_nonpv_nodes_searched)++;
#if LOG_LEVEL >= 2
    uint64_t savedNumNodesSearched = _stats.nodes_searched;
#endif

    Value standpat = -VALUE_INFINITE;
    Value bestValue = -VALUE_INFINITE;
    if (!is_in_check)
    {
        standpat = bestValue = _scorer.score(position);
        LOG_DEBUG("[%d] POSITION score=%ld", info->_ply, standpat);

        if (standpat >= beta)
        {
            EXIT_QSEARCH(standpat);
        }

        if (PV_NODE && standpat > alpha) alpha = standpat;
    }

    Move* begin = MOVE_LIST[info->_ply];
    Move* end = generate_moves(position, position.color(), begin);
    const int n_moves = end - begin;

    if (n_moves == 0) EXIT_QSEARCH(is_in_check ? lost_in(0) : VALUE_DRAW);

    _move_orderer.order_moves(position, begin, end, info);

    int non_quiet_move_count = 0;

    for (int move_count = 0; move_count < n_moves; ++move_count)
    {
        Move move = begin[move_count];
        info->_current_move = move;
        info->_counter_move =
            &_counter_move_table[position.piece_at(from(move))][to(move)];

        const bool move_is_quiet = position.move_is_quiet(move);
        non_quiet_move_count += int(!move_is_quiet);

        // consider only queen promotions
        if (!is_in_check && promotion(move) != NO_PIECE_KIND && promotion(move) != QUEEN)
            continue;

        // don't consider captures with negative SEE
        if (!is_in_check && position.move_is_capture(move) && depth < MAX_DEPTH - 1 && position.see(move) < PIECE_VALUE[KNIGHT].eg)
            continue;

        // only consider first two quiet check evasions
        if (move_is_quiet && (!is_in_check || non_quiet_move_count > 2))
            continue;

        LOG_DEBUG("[%d] DO MOVE %s alpha=%ld beta=%ld", info->_ply,
                  position.uci(move).c_str(), alpha, beta);
        MoveInfo moveinfo = position.do_move(move);

        Value result = -quiescence_search(position, depth - 1, -(alpha + 1),
                                          -alpha, info + 1);
        if (PV_NODE && alpha < result && result < beta)
        {
            result = -quiescence_search(position, depth - 1, -beta, -alpha,
                                        info + 1);
        }

        position.undo_move(move, moveinfo);
        LOG_DEBUG("[%d] UNDO MOVE %s", info->_ply,
                  position.uci(move).c_str());

        if (is_mate(result))
        {
            result += result > VALUE_DRAW ? -1 : 1;
        }

        if (result > bestValue)
        {
            bestValue = result;
            if (result > alpha)
            {
                add_new_move_to_pv_list(info, move, info + 1);
                alpha = result;
                if (result >= beta)
                {
                    LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
                    EXIT_QSEARCH(result);
                }
            }
        }
    }

    LOG_DEBUG("[%d] NODES SEARCHED %lu", info->_ply, _stats.nodes_searched - savedNumNodesSearched);
    EXIT_QSEARCH(bestValue);
}

bool Search::check_limits()
{
    check_limits_counter--;
    if (check_limits_counter > 0) return false;

    check_limits_counter = 40960;

    if (_stats.nodes_searched >= _max_nodes_searched)
    {
        stop_search = true;
        return true;
    }

    TimePoint end_time = std::chrono::steady_clock::now();
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                          end_time - _start_time)
                          .count();

    if (elapsed >= _search_time)
    {
        stop_search = true;
        return true;
    }

    return false;
}

}  // namespace engine
