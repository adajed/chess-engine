#include <gtest/gtest.h>

#include "move_bitboards.h"
#include "zobrist_hash.h"

int main(int argc, char** argv)
{
    using namespace engine;

    testing::InitGoogleTest(&argc, argv);

    init_move_bitboards();
    init_zobrist();

    return RUN_ALL_TESTS();
}
