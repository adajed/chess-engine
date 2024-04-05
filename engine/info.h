#ifndef CHESS_ENGINE_INFO_H_
#define CHESS_ENGINE_INFO_H_

#include "types.h"
#include "value.h"

#include <array>

namespace engine
{

template <typename T, int size1, int size2>
using Array2D = std::array<std::array<T, size2>, size1>;

template <typename T, int size1, int size2, int size3>
using Array3D = std::array<std::array<std::array<T, size3>, size2>, size1>;

using PieceHistory = Array2D<int, PIECE_NUM, SQUARE_NUM>;

using HistoryScore = Array3D<int, COLOR_NUM, SQUARE_NUM, SQUARE_NUM>;

struct Info
{
    Info()
        : _pv_list_length(0),
          _static_eval(VALUE_NONE),
          _ply(0),
          _current_move(NO_MOVE),
          _killer_moves{NO_MOVE, NO_MOVE},
          _counter_move(nullptr)
    {
    }

    std::array<Move, MAX_DEPTH * 2> _pv_list;
    int _pv_list_length;
    Value _static_eval;
    int _ply;
    Move _current_move;
    Move _killer_moves[2];
    PieceHistory* _counter_move;
};

using StackInfo = std::array<Info, MAX_DEPTH * 2 + 2>;

} /* namespace engine */

#endif /* end of include guard: CHESS_ENGINE_INFO_H_ */
