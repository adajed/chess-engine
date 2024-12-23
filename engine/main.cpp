#include "endgame.h"
#include "movegen.h"
#include "uci.h"

using namespace engine;

int main()
{
    move_bitboards::init();
    bitbase::init();
    endgame::init();

    Uci uci;
    uci.loop();

    return 0;
}
