#include "uci.h"

#include <thread>

#include "logger.h"
#include "syzygy/tbprobe.h"
#include "transposition_table.h"
#include "chessplusplusConfig.h"

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

namespace engine
{
Uci::Uci() : search(nullptr), position(), quit(false), options(), polyglot(), polyglot_sample_random_move(true)
{
    options["Polyglot Book"] = UciOption("", [this](std::string path) {
        if (path == "")
            this->polyglot = PolyglotBook();
        else
            this->polyglot = PolyglotBook(path);
    });
    options["Polyglot Sample"] = UciOption("random", {"random", "best"},
            [this](std::string option) {
                this->polyglot_sample_random_move = option == "random";
            });
    options["Logfile"] = UciOption("", [](std::string path) {
        if (path == "")
            logger.close_file();
        else
            logger.open_file(path);
    });

    options["SyzygyPath"] = UciOption("", [](std::string path) {
        Tablebases::init(path);
    });
    options["SyzygyProbeDepth"] = UciOption(1, 1, 100, [](int syzygyProbeDepth) {
        Tablebases::ProbeDepth = syzygyProbeDepth;
    });
    options["Syzygy50MoveRule"] = UciOption(true, [](bool syzygy50MoveRule) {
        Tablebases::Use50Rule = syzygy50MoveRule;
    });
    options["SyzygyProbeLimit"] = UciOption(7, 0, 7, [](int syzygyProbeLimit) {
        Tablebases::ProbeLimit = syzygyProbeLimit;
    });
}

void Uci::loop()
{
    logger << IO_LOCK << LOGGER_OUTPUT << "Chess engine by Adam Jedrych"
              << " (build " << __DATE__ << " " << __TIME__ << ")" << sync_endl;

    position = Position();
    quit = false;

    std::string line;
    std::string token;

    while (!quit && std::getline(std::cin, line))
    {
        // handle empty lines
        if (line == "")
            continue;

        logger << LOGGER_INPUT << line << std::endl;
        std::istringstream istream(line);
        istream >> token;

        bool b = false;

#define COMMAND(name)                \
    if (token == #name)              \
    {                                \
        b = name##_command(istream); \
    }                                \
    else

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
        COMMAND(moves)
        COMMAND(staticeval)

#undef COMMAND

        if (!b)
        {
            logger_sync_out << "Unknown command" << sync_endl;
        }
    }
}

bool Uci::uci_command(std::istringstream& /* istream */)
{
    logger_sync_out << "id name " << ENGINE_NAME << " "
              << CHESSPLUSPLUS_VERSION;
#ifdef TAG
    logger << " " << STRINGIFY(TAG);
#endif
    logger << sync_endl;
    logger_sync_out << "id author Adam Jedrych" << sync_endl;
    logger << sync_endl;

    for (auto option_pair : options)
    {
        std::string name = option_pair.first;
        UciOption option = option_pair.second;
        OptionType optiontype = option.get_type();

        logger_sync_out << "option ";
        logger << "name " << name << " ";
        logger << "type " << optiontype_to_string(optiontype) << " ";
        if (optiontype == kCHECK)
            logger << "default " << (option.get_check() ? "true" : "false")
                   << " ";
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

        logger << sync_endl;
    }

    logger_sync_out << "uciok" << sync_endl;
    return true;
}

bool Uci::ucinewgame_command(std::istringstream& /* istream */)
{
    position = Position();
    scorer.clear();
    ttable.clear();
    return true;
}

bool Uci::isready_command(std::istringstream& /* istream */)
{
    logger_sync_out << "readyok" << sync_endl;
    return true;
}

bool Uci::setoption_command(std::istringstream& istream)
{
    std::string token;
    istream >> token;

    if (token != "name") return false;

    std::string name = "";

    while (istream >> token && token != "value") name += token + " ";
    name.pop_back();  // remove last space

    if (options.find(name) == options.end()) return false;

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
        while (istream >> token && token != "moves") fen += token + " ";

        position = Position(fen);
    }
    else
        return false;

    while (istream >> token)
    {
        if (token == "moves") continue;

        position.do_move(position.parse_uci(token));
    }

    ttable.updateEpoch(1);

    return true;
}

bool Uci::staticeval_command(std::istringstream& /* istream */)
{
    logger_sync_out << position << std::endl << std::endl;

    PositionScorer scorer;
    Value score = scorer.score(position);
    logger << "Score: " << score2str(score) << sync_endl;
    scorer.print_stats();
    return true;
}

bool Uci::moves_command(std::istringstream& istream)
{
    std::string token;
    while (istream >> token)
    {
        position.do_move(position.parse_uci(token));
    }

    ttable.updateEpoch(1);

    return true;
}

void start_searching(Uci* uci)
{
    uint64_t key = PolyglotBook::hash(uci->position);
    if (uci->polyglot.contains(key))
    {
        Move move = uci->polyglot_sample_random_move 
            ? uci->polyglot.get_random_move(key, uci->position)
            : uci->polyglot.get_best_move(key, uci->position);
        logger_sync_out << "bestmove " << uci->position.uci(move) << sync_endl;
    }
    else
        uci->search->go();
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
                limits.searchmoves[limits.searchmovesnum++] =
                    position.parse_uci(token);
        }
    }

    search = std::make_shared<Search>(position, limits, scorer, ttable);

    std::thread search_thread(start_searching, this);
    search_thread.detach();

    return true;
}

bool Uci::stop_command(std::istringstream& /* istream */)
{
    if (search) search->stop();
    return true;
}

bool Uci::ponderhit_command(std::istringstream& /* istream */)
{
    return true;
}

bool Uci::quit_command(std::istringstream& /* istream */)
{
    if (search) search->stop();
    quit = true;
    return true;
}

bool Uci::printboard_command(std::istringstream& /* istream */)
{
    logger_sync_out << position << sync_endl;
    return true;
}

bool Uci::hash_command(std::istringstream& /* istream */)
{
    logger_sync_out << "Hex: " << std::hex << position.hash() << std::dec
              << sync_endl;
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
        Move* end = generate_moves(position, position.color(), begin);

        for (Move* it = begin; it != end; ++it)
        {
            Move move = *it;
            MoveInfo moveinfo = position.do_move(move);

            uint64_t n = perft(position, depth - 1);

            position.undo_move(move, moveinfo);

            logger_sync_out << position.uci(move) << ": " << n << sync_endl;
            sum += n;
        }
    }

    TimePoint end_time = std::chrono::steady_clock::now();
    uint64_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time)
                            .count();

    logger_sync_out << sync_endl;
    logger_sync_out << "Number of nodes: " << sum << sync_endl;
    logger_sync_out << "Time: " << duration << "ms" << sync_endl;
    logger_sync_out << "Speed: " << sum * 1000LL / (duration + 1) << "nps"
              << sync_endl;

    return true;
}

}  // namespace engine
