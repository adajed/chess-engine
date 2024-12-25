#ifndef CHESS_ENGINE_SEARCH_H_
#define CHESS_ENGINE_SEARCH_H_

#include "move_orderer.h"
#include "position.h"
#include "score.h"
#include "time_manager.h"
#include "transposition_table.h"
#include "types.h"

#include <chrono>
#include <cstdint>

namespace engine
{
using MoveList = std::vector<Move>;
using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

std::string score2str(Value score);

using NodeCount = uint64_t;
constexpr NodeCount MAX_NODE_COUNT = 1ULL << 63;

constexpr Duration INFINITE_DURATION = 1LL << 32;

struct SearchStats
{
    SearchStats()
        : nodes_searched(0)
        , pv_nodes_searched(0)
        , non_pv_nodes_searched(0)
        , quiescence_pv_nodes_searched(0)
        , quiescence_nonpv_nodes_searched(0)
        , tt_hits(0)
        , tb_hits(0)
#if LOG_LEVEL > 0
        , at_depth_nodes_searched{}
#endif
    {
#if LOG_LEVEL > 0
        for (int d = 0 ; d < MAX_DEPTH; ++d)
            at_depth_nodes_searched[d] = 0;
#endif
    }

    /**
     * @brief Total number of nodes searched.
     */
    NodeCount nodes_searched;
    /**
     * @brief Number of PV nodes searched.
     */
    NodeCount pv_nodes_searched;
    /**
     * @brief Number of nonPV nodes searched.
     */
    NodeCount non_pv_nodes_searched;
    /**
     * @brief Number of quiescence PV nodes searched.
     */
    NodeCount quiescence_pv_nodes_searched;
    /**
     * @brief Number of quiescence nonPV nodes searched.
     */
    NodeCount quiescence_nonpv_nodes_searched;
    /**
     * @brief Number of ttable hits.
     */
    uint64_t tt_hits;
    /**
     * @brief Number of tablebase hits.
     */
    uint64_t tb_hits;

#if LOG_LEVEL > 0
    /**
     * @brief Number of nodes searched at given depth.
     */
    std::array<NodeCount, MAX_DEPTH> at_depth_nodes_searched;
#endif
};

class Search
{
  public:
    Search(const Position& position, const Limits& limits,
           PositionScorer& scorer, tt::TTable& ttable);

    void go();

    void stop();

  private:
    void init_search();

    void iter_search();


    Value search(Position& position, Depth depth, Value alpha, Value beta,
                 Info* info);

    Value quiescence_search(Position& position, Depth depth, Value alpha,
                            Value beta, Info* info);

    void print_info(Value score, Depth depth, int64_t elapsed, Info* info);

    bool check_limits();

    Position _position;
    PositionScorer& _scorer;
    Limits limits;

    int64_t check_limits_counter;
    bool stop_search;

    Duration _search_time;
    Depth _search_depth;
    Depth _current_depth;
    NodeCount _max_nodes_searched;

    Move _best_move;
    TimePoint _start_time;

    std::vector<Move> _root_moves;

    tt::TTable& _ttable;
    StackInfo _stack_info;
    HistoryScore _history_score;
    MoveOrderer _move_orderer;
    Array2D<PieceHistory, PIECE_NUM, SQUARE_NUM> _counter_move_table;

    SearchStats _stats;
};

}  // namespace engine

#endif  // CHESS_ENGINE_SEARCH_H_
