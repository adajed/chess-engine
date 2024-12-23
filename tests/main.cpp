#include <gtest/gtest.h>

#include "endgame.h"
#include "move_bitboards.h"

int main(int argc, char** argv)
{
    using namespace engine;

    testing::InitGoogleTest(&argc, argv);

    move_bitboards::init();
    bitbase::init();
    endgame::init();

    return RUN_ALL_TESTS();
}
