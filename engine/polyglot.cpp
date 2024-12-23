#include "polyglot.h"
#include "hash_position.h"

#include <chrono>
#include <fstream>

namespace engine
{

PolyglotBook::PolyglotBook(std::string path) : PolyglotBook(path, std::chrono::system_clock::now().time_since_epoch().count()) { }

PolyglotBook::PolyglotBook(std::string path, size_t seed)
    : _hashmap(),
      _gen(seed),
      _dist(),
      _seed(seed)
{
    std::ifstream stream(path, std::ios::binary);
    stream.seekg(0);

    char entry[16];

    while (stream)
    {
        stream.read(entry, 16);

        uint64_t key = (((uint64_t)entry[0] & 0xFF) << 56) |
                       (((uint64_t)entry[1] & 0xFF) << 48) |
                       (((uint64_t)entry[2] & 0xFF) << 40) |
                       (((uint64_t)entry[3] & 0xFF) << 32) |
                       (((uint64_t)entry[4] & 0xFF) << 24) |
                       (((uint64_t)entry[5] & 0xFF) << 16) |
                       (((uint64_t)entry[6] & 0xFF) << 8) |
                       (((uint64_t)entry[7] & 0xFF));

        uint16_t move_code = (entry[8] & 0xFF) << 8 | (entry[9] & 0xFF);

        Rank fromRank = Rank((move_code >> 9) & 0x7);
        File fromFile = File((move_code >> 6) & 0x7);
        Rank toRank = Rank((move_code >> 3) & 0x7);
        File toFile = File(move_code & 0x7);
        int promotion_code = (move_code >> 12) & 0x7;

        PieceKind promotion = NO_PIECE_KIND;
        if (promotion_code != 0) promotion = PAWN + PieceKind(promotion_code);

        Move move = create_promotion(make_square(fromRank, fromFile),
                                     make_square(toRank, toFile), promotion);
        int weight = (entry[10] & 0xFF) << 8 | (entry[11] & 0xFF);

        if (_hashmap.find(key) == _hashmap.end())
            _hashmap[key] = std::vector<WeightedMove>();

        _hashmap[key].push_back(std::make_pair(move, weight));
    }
}

uint64_t PolyglotBook::hash(const Position& position)
{
    return hash_position(position);
}

bool PolyglotBook::contains(uint64_t key) const
{
    return _hashmap.find(key) != _hashmap.end();
}

Move PolyglotBook::get_random_move(uint64_t key, const Position& position) const
{
    assert(contains(key));

    const std::vector<WeightedMove>& moves = _hashmap.at(key);
    int sum_of_weights = 0;
    for (WeightedMove wmove : moves) sum_of_weights += wmove.second;

    assert(sum_of_weights > 0);

    int sample = _dist(_gen) % sum_of_weights;
    int w = 0;
    uint32_t i = 0;
    while (i < moves.size() && w + moves[i].second < sample)
        w += moves[i++].second;

    return decode_move(moves[i].first, position);
}

Move PolyglotBook::get_best_move(uint64_t key, const Position& position) const
{
    assert(contains(key));

    const std::vector<WeightedMove>& moves = _hashmap.at(key);

    WeightedMove best_move = *std::max_element(moves.begin(), moves.end(),
            [](const auto& m1, const auto& m2) { return m1.second < m2.second; });

    return decode_move(best_move.first, position);
}

Move PolyglotBook::decode_move(Move move, const Position& position) const
{
    if (from(move) == SQ_E1 && (to(move) == SQ_H1 || to(move) == SQ_G1) &&
        position.piece_at(from(move)) == W_KING)
        return create_castling(KING_CASTLING);
    if (from(move) == SQ_E1 && (to(move) == SQ_A1 || to(move) == SQ_C1) &&
        position.piece_at(from(move)) == W_KING)
        return create_castling(QUEEN_CASTLING);
    if (from(move) == SQ_E8 && (to(move) == SQ_H8 || to(move) == SQ_G8) &&
        position.piece_at(from(move)) == B_KING)
        return create_castling(KING_CASTLING);
    if (from(move) == SQ_E8 && (to(move) == SQ_A8 || to(move) == SQ_C8) &&
        position.piece_at(from(move)) == B_KING)
        return create_castling(QUEEN_CASTLING);
    return move;
}

}  // namespace engine
