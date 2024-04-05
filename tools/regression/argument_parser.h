#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using EngineOption = std::pair<std::string, std::optional<std::string>>;

struct TimeFormat
{
    int time_initial_ms;
    int time_increment_ms;
};

struct EngineParams
{
    std::string command;
    std::string name;
    std::vector<EngineOption> options;
};

struct Args
{
    Args()
        : engines(),
          game_format({{TimeFormat{300000, 3000}, 1}}),
          debug(false),
          repeat(false),
          polyglot(""),
          book_depth(0),
          pgn_file(),
          num_threads(1),
          seed(std::chrono::system_clock::now().time_since_epoch().count())
    {
    }

    int getTotalNumGames() const
    {
        int total = 0;
        for (const auto& p : game_format)
        {
            total += p.second;
        }
        return total;
    }

    std::vector<EngineParams> engines;
    std::vector<std::pair<TimeFormat, int>> game_format;
    bool debug;
    bool repeat;
    std::string polyglot;
    int book_depth;
    std::string pgn_file;
    int num_threads;
    size_t seed;
};

std::pair<TimeFormat, int> parse_single_time_format(std::string str)
{
    size_t pos = str.find(":");
    std::string time_str = str.substr(0, pos);
    int num_games = std::stoi(str.substr(pos + 1));

    pos = time_str.find("+");
    int time_initial_ms = std::stoi(time_str.substr(0, pos)) * 60 * 1000;
    int time_increment_ms = std::stoi(time_str.substr(pos + 1)) * 1000;

    return {TimeFormat{time_initial_ms, time_increment_ms}, num_games};
}

std::vector<std::pair<TimeFormat, int>> parse_game_format(std::string str)
{
    std::vector<std::pair<TimeFormat, int>> result;

    size_t pos = 0;
    std::string token;
    while ((pos = str.find(",")) != std::string::npos)
    {
        token = str.substr(0, pos);
        result.push_back(parse_single_time_format(token));
        str.erase(0, pos + 1);
    }
    result.push_back(parse_single_time_format(str));

    return result;
}

inline Args parse_args(int argc, char** argv)
{
    std::vector<std::string> tokens;
    for (int i = 0; i < argc; ++i)
    {
        tokens.push_back(std::string(argv[i]));
    }

    if (std::find(tokens.begin(), tokens.end(), "--help") != tokens.end())
    {
        std::cout << "Usage: " << tokens[0] << " [option [option ...]]" << std::endl << std::endl;

        std::cout << "Options:" << std::endl;
        std::cout << "\t--engine [engineOption [engineOption ...]] - Adds new engine with given parameters to regression" << std::endl;
        std::cout << "\t--format - Format of games played: time:numgames[,time:numgames[,..]] (default: 5+3:1)" << std::endl;
        std::cout << "\t--threads - Number of threads to use (default: 1)" << std::endl;
        std::cout << "\t--pgn - Path to PGN files where games will be saved (default: games.pgn)" << std::endl;
        std::cout << "\t--polyglot - Path to polyglot book used for openings (default: not used)" << std::endl;
        std::cout << "\t--bookdepth - Depth (in plies) up to which the book is used (default: 0)" << std::endl;
        std::cout << "\t--repeat - Repeat opening for both sides" << std::endl;
        std::cout << "\t--debug - Print debug messages" << std::endl;
        std::cout << std::endl;

        std::cout << "Engine options:" << std::endl;
        std::cout << "\tcommand=<command> - command used to run engine" << std::endl;
        std::cout << "\tname=<name> - name of the engine" << std::endl;
        std::cout << "\toption=<name>[:<value>] - option for the engine" << std::endl;
        std::cout << std::endl;
        exit(0);
    }

    Args args;

    int pos = 1;
    while (pos < argc)
    {
        if (tokens[pos] == "--engine" || tokens[pos] == "-e")
        {
            EngineParams params;
            pos++;
            while (pos < argc && tokens[pos].find("-") != 0)
            {
                size_t idx = tokens[pos].find("=");
                std::string cmd = tokens[pos].substr(0, idx);
                std::string value = tokens[pos].substr(idx + 1);
                if (cmd == "command")
                {
                    params.command = value;
                }
                else if (cmd == "name")
                {
                    params.name = value;
                }
                else if (cmd == "option")
                {
                    bool contains_value = value.find_first_of(':') != std::string::npos;
                    std::string option_name = contains_value
                        ? value.substr(0, value.find_first_of(':'))
                        : value;
                    std::optional<std::string> option_value = contains_value
                        ? std::optional<std::string>{value.substr(value.find_first_of(':') + 1)}
                        : std::nullopt;
                    params.options.push_back(std::make_pair(option_name, option_value));
                }
                else
                {
                    std::cerr << "Unknown engine param: " << cmd << std::endl;
                }

                pos++;
            }

            args.engines.push_back(params);
        }
        else if (tokens[pos] == "--format" || tokens[pos] == "-f")
        {
            pos++;
            args.game_format = parse_game_format(tokens[pos]);
            pos++;
        }
        else if (tokens[pos] == "--threads")
        {
            pos++;
            args.num_threads = std::stoi(tokens[pos++]);
        }
        else if (tokens[pos] == "--debug")
        {
            pos++;
            args.debug = true;
        }
        else if (tokens[pos] == "--repeat")
        {
            pos++;
            args.repeat = true;
        }
        else if (tokens[pos] == "--polyglot" || tokens[pos] == "-p")
        {
            pos++;
            args.polyglot = tokens[pos++];
        }
        else if (tokens[pos] == "--bookdepth")
        {
            pos++;
            args.book_depth = std::stoi(tokens[pos++]);
        }
        else if (tokens[pos] == "--pgn")
        {
            pos++;
            args.pgn_file = tokens[pos++];
        }
        else if (tokens[pos] == "--seed")
        {
            pos++;
            args.seed = std::stoll(tokens[pos++]);
        }
        else
        {
            std::cerr << "Unknown argument: " << tokens[pos] << std::endl;
            pos++;
        }
    }

    return args;
}
