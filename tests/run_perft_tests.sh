#!/bin/bash

trap "{ exit 255; }" INT

PROGRAM=${1:-./build/chessplusplus}

run_perft_test()

{
    fen=$1
    depth=$2
    nodes=$3

    NUMBER_OF_PERFT_TESTS=$((NUMBER_OF_PERFT_TESTS + 1))
    expect tests/scripts/perft.exp ${PROGRAM} "${fen}" ${depth} ${nodes} >/dev/null
    if [ $? -eq 0 ]
    then
        PERFT_TESTS_PASSED=$((PERFT_TESTS_PASSED + 1))
        echo "Perft test \"${fen}\" ${depth} passed"
    else
        echo "Perft test \"${fen}\" ${depth} failed"
    fi
}

NUMBER_OF_PERFT_TESTS=0
PERFT_TESTS_PASSED=0

run_perft_test "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 1 20
run_perft_test "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 2 400
run_perft_test "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 3 8902
run_perft_test "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 4 197281
run_perft_test "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 5 4865609
run_perft_test "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 6 119060324


run_perft_test "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - " 1 48
run_perft_test "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - " 2 2039
run_perft_test "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - " 3 97862
run_perft_test "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - " 4 4085603
run_perft_test "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - " 5 193690690


run_perft_test "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - " 1 14
run_perft_test "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - " 2 191
run_perft_test "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - " 3 2812
run_perft_test "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - " 4 43238
run_perft_test "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - " 5 674624
run_perft_test "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - " 6 11030083
run_perft_test "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - " 7 178633661


run_perft_test "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - " 1 6
run_perft_test "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - " 2 264
run_perft_test "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - " 3 9467
run_perft_test "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - " 4 422333
run_perft_test "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - " 5 15833292
run_perft_test "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - " 6 706045033


run_perft_test "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 1 6
run_perft_test "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 2 264
run_perft_test "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 3 9467
run_perft_test "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 4 422333
run_perft_test "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 5 15833292
run_perft_test "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 6 706045033


run_perft_test "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - " 1 44
run_perft_test "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - " 2 1486
run_perft_test "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - " 3 62379
run_perft_test "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - " 4 2103487
run_perft_test "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - " 5 89941194


run_perft_test "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - " 1 46
run_perft_test "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - " 2 2079
run_perft_test "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - " 3 89890
run_perft_test "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - " 4 3894594
run_perft_test "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - " 5 164075551

run_perft_test "r6r/1b2k1bq/8/8/7B/8/8/R3K2R b KQ - 3 2" 1 8
run_perft_test "8/8/8/2k5/2pP4/8/B7/4K3 b - d3 0 3" 1 8
run_perft_test "r1bqkbnr/pppppppp/n7/8/8/P7/1PPPPPPP/RNBQKBNR w KQkq - 2 2" 1 19
run_perft_test "r3k2r/p1pp1pb1/bn2Qnp1/2qPN3/1p2P3/2N5/PPPBBPPP/R3K2R b KQkq - 3 2" 1 5
run_perft_test "2kr3r/p1ppqpb1/bn2Qnp1/3PN3/1p2P3/2N5/PPPBBPPP/R3K2R b KQ - 3 2" 1 44
run_perft_test "rnb2k1r/pp1Pbppp/2p5/q7/2B5/8/PPPQNnPP/RNB1K2R w KQ - 3 9" 1 39
run_perft_test "2r5/3pk3/8/2P5/8/2K5/8/8 w - - 5 4" 1 9
run_perft_test "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8" 3 62379
run_perft_test "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10" 3 89890
run_perft_test "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1" 6 1134888
run_perft_test "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1" 6 1015133
run_perft_test "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1" 6 1440467
run_perft_test "5k2/8/8/8/8/8/8/4K2R w K - 0 1" 6 661072
run_perft_test "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1" 6 803711
run_perft_test "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1" 4 1274206
run_perft_test "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1" 4 1720476
run_perft_test "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1" 6 3821001
run_perft_test "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1" 5 1004658
run_perft_test "4k3/1P6/8/8/8/8/K7/8 w - - 0 1" 6 217342
run_perft_test "8/P1k5/K7/8/8/8/8/8 w - - 0 1" 6 92683
run_perft_test "K1k5/8/P7/8/8/8/8/8 w - - 0 1" 6 2217
run_perft_test "8/k1P5/8/1K6/8/8/8/8 w - - 0 1" 7 567584
run_perft_test "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1" 4 23527

echo "Perft tests passed: ${PERFT_TESTS_PASSED}/${NUMBER_OF_PERFT_TESTS}"
if [ ${PERFT_TESTS_PASSED} -ne ${NUMBER_OF_PERFT_TESTS} ]
then
    exit 1
fi

