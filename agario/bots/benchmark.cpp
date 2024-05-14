
#include <agario/engine/Engine.hpp>
#include <agario/bots/bots.hpp>

#include <utils/thread-pool.h>
#include <utils/ostreamlock.h>

#include <mutex>
#include <iostream>
#include <sstream>
#include <map>

#include <cxxopts.hpp>

#define RENDERABLE false


class GameRecap {
public:

  /* records the score from a single player */
  void append(const std::string &name, agario::mass score) {
    scores[name].emplace_back(score);
  }

  /* return the names of all the bots being tested */
  std::vector<std::string> names() const {
    if (scores.empty()) return {};
    std::vector<std::string> names;
    for (auto &pair : scores)
      names.emplace_back(pair.first);
    return names;
  }

  float average(const std::string &name) const {
    float avg = 0;
    int count = 0;
    for (auto mass : scores.at(name)) {
      avg += mass;
      count++;
    }
    return avg / count;
  }

  float max(const std::string &name) const {
    float m = 0;
    for (auto &mass : scores.at(name))
      m = std::max<float>(m, mass);
    return m;
  }

  float min(const std::string &name) const {
    float m = std::numeric_limits<float>::max();
    for (auto &mass : scores.at(name))
      m = std::min<float>(m, mass);
    return m;
  }

private:
  // bot name => list of scores
  std::map<std::string, std::vector<agario::mass>> scores;
};

class GamesRecap {
public:
  GameRecap &new_game_recap() {
    recaps.emplace_back();
    return recaps.back();
  }

  std::vector<std::string> names() const {
    if (recaps.empty()) return {};
    else return recaps.front().names();
  }

  float macro_average(const std::string &name) const {
    return macro_stat(&GameRecap::average, name);
  }

  float macro_maximum(const std::string &name) const {
    return macro_stat(&GameRecap::max, name);
  }

  float macro_minimum(const std::string &name) const {
    return macro_stat(&GameRecap::min, name);
  }

private:
  std::vector<GameRecap> recaps;

  template<typename F>
  float macro_stat(F get_stat, const std::string &name) const {
    float average = 0;
    int count = 0;
    for (auto &recap : recaps) {
      average += (recap.*get_stat)(name);
      count++;
    }
    return average / count;
  }
};

std::ostream &operator<<(std::ostream &os, const GamesRecap &recaps) {
  auto names = recaps.names();

  /* sort by macro average */
  std::sort(names.begin(), names.end(),
            [&](const std::string &n1, const std::string &n2) {
              return recaps.macro_average(n1) > recaps.macro_average(n2);
            });

  // fin the longest name
  std::vector<unsigned long> lens;
  std::transform(names.begin(), names.end(), std::back_inserter(lens),
                 [](const std::string &name) { return name.length(); });
  auto longest = *std::max_element(lens.begin(), lens.end());

  os << std::setw(longest) << "Name";
  os << std::setw(5) << "Avg.";
  os << std::setw(5) << "Max.";
  os << std::setw(5) << "Min." << std::endl;
  os << std::setfill('=') << std::setw(40) << "" << std::setfill(' ') << std::endl;

  for (auto &name : names) {
    os << std::setw(longest) << name;
    os << std::setw(5) << (int) recaps.macro_average(name);
    os << std::setw(5) << (int) recaps.macro_maximum(name);
    os << std::setw(5) << (int) recaps.macro_minimum(name) << std::endl;
  }
  return os;
}

template<typename ...Ts>
class BotEvaluator {
public:
  explicit BotEvaluator(int num_bots, float game_duration,
                        int tick_freq, int threads) :
    num_bots(num_bots), game_duration(game_duration),
    tick_freq(tick_freq), pool(threads) {}

  /**
   * Executes `num_games` number of games, recording results
   * after each of the games
   * @param num_games the number of games to play
   */
  void run(int num_games) {
    int ticks_per_game = static_cast<int>(60 * game_duration * tick_freq);

    for (int game = 0; game < num_games; game++) {
      pool.schedule([=]() {
        std::cout << oslock << "Playing game " << game << std::endl << osunlock;
        agario::Engine<RENDERABLE> engine;
        engine.reset();
        add_bots<Ts...>(engine);

        agario::time_delta dt(1.0 / tick_freq);
        for (int t = 0; t < ticks_per_game; t++)
          engine.tick(dt);

        m.lock();
        record_scores(engine.get_game_state());
        m.unlock();
      });

    }

    pool.wait();
  }

  const GamesRecap &stats() { return recaps; }

private:
  int num_bots;
  float game_duration;
  int tick_freq;

  GamesRecap recaps;

  ThreadPool pool;
  std::mutex m;

  /**
   * Records statistics about the scores after a single game
   * @param state The game state at the end of a single game
   */
  void record_scores(const agario::GameState<RENDERABLE> &state) {
    GameRecap &recap = recaps.new_game_recap();

    for (auto &pair : state.players) {
      auto &player = *pair.second;
      const std::string &name = player.name();
      recap.append(name, player.mass());
    }
  }

  template<typename ...Us>
  void add_bots(agario::Engine<RENDERABLE> &engine) { _add_bots<Us...>(engine); }

  template<>
  void add_bots<>(agario::Engine<RENDERABLE> &engine) {} // recursive base case

  /* add multiple kinds of bots, using the typeid as name */
  template<typename U, typename ...Us>
  void _add_bots(agario::Engine<RENDERABLE> &engine) {
    for (int i = 0; i < num_bots; i++) {
      engine.add_player<U>();
    }
    add_bots<Us...>(engine);
  }

};

// command line option parsing...
cxxopts::Options options() {

  try {
    cxxopts::Options options("Agar.io Bot Evaluator", "command line options");
    options
      .positional_help("[optional args]")
      .show_positional_help();

    options.add_options()
      ("g,games", "number of games", cxxopts::value<int>()->default_value("10"))
      ("b,bots", "number of bots", cxxopts::value<int>()->default_value("7"))
      ("d,duration", "game duration", cxxopts::value<float>()->default_value("3.0"))
      ("f,frequency", "tick frequency", cxxopts::value<int>()->default_value("30"))
      ("j,threads", "number of threads", cxxopts::value<int>()->default_value("4"))
      ("help", "Print help");


    options.parse_positional({"server"});
    return options;

  } catch (const std::exception &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    exit(1);
  }
}

int main(int argc, char *argv[]) {

  // using namespace agario::bot;
  // using HungryBot = HungryBot<RENDERABLE>;
  // using HungryShyBot = HungryShyBot<RENDERABLE>;
  // using AggressiveBot = AggressiveBot<RENDERABLE>;
  // using AggressiveShyBot = AggressiveShyBot<RENDERABLE>;

  // /* configure which bots to evaluate in this template pack */
  // using Evaluator = BotEvaluator<
  //   HungryBot,
  //   HungryShyBot,
  //   AggressiveBot,
  //   AggressiveShyBot>;

  // /* command line parsing and evaluation */

  // auto opts = options();
  // auto args = opts.parse(argc, argv);

  // int num_games = args["games"].as<int>();
  // int num_bots = args["bots"].as<int>();
  // float game_duration = args["duration"].as<float>();
  // int tick_freq = args["frequency"].as<int>();
  // int threads = args["threads"].as<int>();

  // Evaluator evaluator(num_bots, game_duration, tick_freq, threads);
  // evaluator.run(num_games);

  // std::cout << evaluator.stats() << std::endl;

  // return 0;
}