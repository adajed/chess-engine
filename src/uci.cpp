#include "uci.h"
#include "logger.h"
#include "transposition_table.h"

#include <thread>

namespace engine
{

Uci::Uci(const PositionScorer& scorer)
    : scorer(scorer)
    , search(nullptr)
    , position()
    , quit(false)
    , options()
    , polyglot()
    , runner()
{
    options["Hash"] = UciOption(1, 1, 1024,
            [](int size) {
                transposition::init(size);
            });
    options["Clear Hash"] = UciOption(
            [](){
                transposition::clear();
            });
    options["Polyglot Book"] = UciOption("",
            [this](std::string path) {
                if (path == "")
                    this->polyglot = PolyglotBook();
                else
                    this->polyglot = PolyglotBook(path);
            });
    options["Logfile"] = UciOption("",
            [](std::string path) {
                if (path == "")
                    logger.close_file();
                else
                    logger.open_file(path);
            });

    std::vector<int64_t> input1_shape = {1, 8, 8, 12};
    std::vector<int64_t> input2_shape = {1, 2};
    std::vector<int64_t> output_shape = {1, 8, 8, 76};

    TF_Tensor* input1 = TF_AllocateTensor(TF_FLOAT, input1_shape.data(), 4, 1 * 8 * 8 * 12 * sizeof(float));
    TF_Tensor* input2 = TF_AllocateTensor(TF_FLOAT, input2_shape.data(), 2, 1 * 2 * sizeof(float));
    TF_Tensor* output = TF_AllocateTensor(TF_FLOAT, output_shape.data(), 2, 1 * 8 * 8 * 76 * sizeof(float));

    runner = tensorflow::Runner(
            "/home/adam/Projects/rlchess/move_net.pb",
            {{"Placeholder", 0, input1}, {"Placeholder_2", 0, input2}},
            {{"Softmax", 0, output}});
}

void Uci::loop()
{
    logger << "Chess engine by Adam Jedrych"
           << " (build " << __DATE__ << " " << __TIME__ << ")" << std::endl;

    position = Position();
    quit = false;

    std::string line;
    std::string token;

    while (!quit && std::getline(std::cin, line))
    {
        logger.fout << line << std::endl;
        std::istringstream istream(line);
        istream >> token;

        bool b = false;

#define COMMAND(name)                       \
        if (token == #name)                 \
        {                                   \
            b = name##_command(istream);    \
        }                                   \

        COMMAND(uci)
        COMMAND(ucinewgame)
        COMMAND(isready)
        COMMAND(setoption)
        COMMAND(position)
        COMMAND(go)
        COMMAND(stop)
        COMMAND(ponderhit)
        COMMAND(quit)
        COMMAND(printboard)
        COMMAND(hash)
        COMMAND(perft)
        COMMAND(scoremoves)

#undef COMMAND

        if (!b)
        {
            logger << "Unknown command" << std::endl;
        }
    }
}

bool Uci::uci_command(std::istringstream& istream)
{
    logger << "id name Deep Chess" << std::endl;
    logger << "id author Adam Jedrych" << std::endl;
    logger << std::endl;

    for (auto option_pair : options)
    {
        std::string name = option_pair.first;
        UciOption option = option_pair.second;
        OptionType optiontype = option.get_type();

        logger << "option ";
        logger << "name " << name << " ";
        logger << "type " << optiontype_to_string(optiontype) << " ";
        if (optiontype == kCHECK)
            logger << "default " << (option.get_check() ? "true" : "false") << " ";
        else if (optiontype == kSPIN)
        {
            logger << "default " << option.get_spin_initial() << " ";
            logger << "min " << option.get_spin_min() << " ";
            logger << "max " << option.get_spin_max() << " ";
        }
        else if (optiontype == kCOMBO)
        {
            logger << "default " << option.get_string() << " ";
            for (std::string s : option.get_combo_options())
                logger << "var " << s << " ";
        }
        else if (optiontype == kSTRING)
            logger << "default " << option.get_string() << " ";

        logger << std::endl;
    }

    logger << "uciok" << std::endl;
    return true;
}

bool Uci::ucinewgame_command(std::istringstream& istream)
{
    position = Position();
    return true;
}

bool Uci::isready_command(std::istringstream& istream)
{
    logger << "readyok" << std::endl;
    return true;
}

bool Uci::setoption_command(std::istringstream& istream)
{
    std::string token;
    istream >> token;

    if (token != "name")
        return false;

    std::string name = "";

    while (istream >> token && token != "value")
        name += token + " ";
    name.pop_back(); // remove last space

    if (options.find(name) == options.end())
        return false;


    OptionType optiontype = options[name].get_type();

    if (optiontype == kCHECK)
    {
        istream >> token;
        options[name].set(token == "true");
    }
    else if (optiontype == kSPIN)
    {
        int value;
        istream >> value;
        options[name].set(value);
    }
    else if (optiontype == kCOMBO)
    {
        istream >> token;
        options[name].set(token);
    }
    else if (optiontype == kBUTTON)
    {
        options[name].set();
    }
    else if (optiontype == kSTRING)
    {
        istream >> token;
        options[name].set(token);
    }

    return true;
}

bool Uci::position_command(std::istringstream& istream)
{
    std::string token;
    istream >> token;

    if (token == "startpos")
    {
        position = Position();
    }
    else if (token == "fen")
    {
        std::string fen = "";
        while (istream >> token && token != "moves")
            fen += token + " ";

        position = Position(fen);
    }
    else
        return false;

    while (istream >> token)
    {
        if (token == "moves")
            continue;

        position.do_move(position.parse_move(token));
    }

    return true;
}

bool Uci::go_command(std::istringstream& istream)
{
    Limits limits;
    std::string token;

    while (istream >> token)
    {
        if (token == "ponder")
            limits.ponder = true;
        else if (token == "wtime")
            istream >> limits.timeleft[WHITE];
        else if (token == "btime")
            istream >> limits.timeleft[BLACK];
        else if (token == "winc")
            istream >> limits.timeinc[WHITE];
        else if (token == "binc")
            istream >> limits.timeinc[BLACK];
        else if (token == "movestogo")
            istream >> limits.movestogo;
        else if (token == "depth")
            istream >> limits.depth;
        else if (token == "nodes")
            istream >> limits.nodes;
        else if (token == "mate")
            istream >> limits.mate;
        else if (token == "movetime")
            istream >> limits.movetime;
        else if (token == "infinite")
            limits.infinite = true;
        else if (token == "searchmoves")
        {
            while (istream >> token)
                limits.searchmoves[limits.searchmovesnum++] = position.parse_move(token);
        }
    }

    search = std::make_shared<Search>(position, scorer, limits, runner);

    std::thread search_thread(
            [this](){
                HashKey key = PolyglotBook::hash(this->position);
                if (this->polyglot.contains(key))
                {
                    Move move = this->polyglot.sample_move(key, this->position);
                    logger << "bestmove " << this->position.move_to_string(move) << std::endl;
                }
                else
                    this->search->go();
            });
    search_thread.detach();

    return true;
}

bool Uci::stop_command(std::istringstream& istream)
{
    if (search)
        search->stop();
    return true;
}

bool Uci::ponderhit_command(std::istringstream& istream)
{
    return true;
}

bool Uci::quit_command(std::istringstream& istream)
{
    if (search)
        search->stop();
    quit = true;
    return true;
}

bool Uci::printboard_command(std::istringstream& istream)
{
    logger << position << std::endl;
    return true;
}

bool Uci::hash_command(std::istringstream& istream)
{
    logger << "Hex: " << std::hex << position.hash() << std::dec << std::endl;
    return true;
}

bool Uci::perft_command(std::istringstream& istream)
{
    int depth;
    istream >> depth;

    TimePoint start_time = std::chrono::steady_clock::now();

    uint64_t sum = 0;
    if (depth > 0)
    {
        Move* begin = MOVE_LIST[depth];
        Move* end = generate_moves(position, position.side_to_move(), begin);

        for (Move* it = begin; it != end; ++it)
        {
            Move move = *it;
            MoveInfo moveinfo = position.do_move(move);

            uint64_t n = perft(position, depth - 1);

            position.undo_move(move, moveinfo);

            logger << position.move_to_string(move) << ": " << n << std::endl;
            sum += n;
        }
    }

    TimePoint end_time = std::chrono::steady_clock::now();
    uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    logger << std::endl;
    logger << "Number of nodes: " << sum << std::endl;
    logger << "Time: " << duration << "ms" << std::endl;
    logger << "Speed: " << sum * 1000LL / duration << "nps" << std::endl;


    return true;
}

bool Uci::scoremoves_command(std::istringstream& istream)
{
    position.fill_buffers(runner.get_input_buffer(0), runner.get_input_buffer(1));
    runner.run();
    float* prob = runner.get_output_buffer(0);

    std::vector<Move> moves(MAX_MOVES, NO_MOVE);
    Move* begin = moves.data();
    Move* end = generate_moves(position, position.side_to_move(), begin);
    std::vector<std::pair<float, Move>> scores(end - begin);
    for (int i = 0; i < end - begin; ++i)
        scores[i] = std::make_pair(prob[position.move_to_pos(begin[i])], begin[i]);
    std::stable_sort(scores.begin(), scores.end(), std::greater<>());

    int pos = 0;
    float m = 0.;
    for (int i = 0; i < 1 * 8 * 8 * 76; ++i)
    {
        if (prob[i] > m)
        {
            m = prob[i];
            pos = i;
        }
    }

    logger << "max(prob[" << pos << "]) : " << m << std::endl;
    for (const auto& p : scores)
        logger << position.move_to_string(p.second) << "[" << position.move_to_pos(p.second) << "] : " << p.first << std::endl;

    return true;
}

}
