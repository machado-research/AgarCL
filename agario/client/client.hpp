#pragma once

#include <agario/rendering/renderer.hpp>
#include <agario/rendering/window.hpp>

#include <agario/core/renderables.hpp>
#include <agario/engine/Engine.hpp>

#include <agario/bots/bots.hpp>

#include <chrono>

#include <string>
#include <ctime>
#include <memory>

#define WINDOW_NAME "AgarIO"
#define DEFAULT_SCREEN_WIDTH 640
#define DEFAULT_SCREEN_HEIGHT 480

#define RENDERABLE true

namespace agario {

  class Client {
  public:
    typedef Player<true> Player;
    typedef Cell<true> Cell;
    typedef Pellet<true> Pellet;
    typedef Food<true> Food;
    typedef Virus<true> Virus;

    Client() :
      server(), port(),
      engine(DEFAULT_ARENA_WIDTH, DEFAULT_ARENA_HEIGHT),
      renderer(nullptr) {
      engine.initialize_game();
    }

    Client(std::string server, int port) :
      server(std::move(server)), port(port), renderer(nullptr) {}

    void connect() {
      std::cout << "Connecting to: " << server << ":" << port << "..." << std::endl;
      // todo: set arena_height and width form server (among other things)
      std::cout << "remote server connection not implemented yet." << std::endl;

    }

    template<typename... Args>
    void set_player(agario::pid pid) {
      player_pid = pid;
    }

    agario::pid add_player(const std::string &name) {
      return engine.add_player<Player>(name);
    }

    void add_bots() {
      using namespace agario::bot;
      using HungryBot = HungryBot<RENDERABLE>;
      using HungryShyBot = HungryShyBot<RENDERABLE>;
      using AggressiveBot = AggressiveBot<RENDERABLE>;
      using AggressiveShyBot = AggressiveShyBot<RENDERABLE>;

      int n = 7;
      range_bot_pids = std::make_pair(1e9,0);
      add_bot<HungryBot>(n);
      add_bot<HungryShyBot>(n);
      add_bot<AggressiveBot>(n);
      add_bot<AggressiveShyBot>(n);
      std::cout << "RANGE BOTS: " << range_bot_pids.first << " " << range_bot_pids.second << std::endl;
    }

    void initialize_renderer() {
      window = std::make_shared<Window>(WINDOW_NAME, DEFAULT_SCREEN_WIDTH, DEFAULT_SCREEN_HEIGHT);
      renderer = std::make_unique<agario::Renderer>(window,
                                                    engine.arena_width(),
                                                    engine.arena_height());
    }

    void play() {
      if (renderer == nullptr) initialize_renderer();

      auto beginning = std::chrono::system_clock::now();
      auto ticks_before = engine.ticks();

      game_loop();

      auto now = std::chrono::system_clock::now();
      std::chrono::duration<double> total_time = now - beginning;

      std::cout << "Game time: " << total_time.count() << " sec." << std::endl;

      float fps = (engine.ticks() - ticks_before) / total_time.count();
      std::cout << "Average FPS: " << fps << std::endl;

      std::cout << "Leader-board:" << std::endl;
      std::cout << engine.game_state() << std::endl;

      window->destroy();
    }

    void game_loop() {
      auto before = std::chrono::system_clock::now();
      while (!window->should_close()) {

        auto now = std::chrono::system_clock::now();
        auto dt = now - before;
        before = now;
        range_bot_pids = std::make_pair(std::min(range_bot_pids.first,player_pid), std::max(range_bot_pids.second,player_pid));
        for (agario::pid i = range_bot_pids.first; i <= range_bot_pids.second; i++) {
          auto &player = engine.player(i);
          if (player.dead()) {
            std::cout << "Player \"" << player.name() << "\" (pid ";
            std::cout << player.pid() << ") died." << std::endl;
            engine.respawn(i);
          }
        }
        auto &player = engine.player(player_pid);

        // if (player.dead()) {
        //   std::cout << "Player \"" << player.name() << "\" (pid ";
        //   std::cout << player.pid() << ") died." << std::endl;
        //   engine.respawn(player_pid);
        // }

        process_input();
        renderer->render_screen(player, engine.game_state());

        glfwPollEvents();
        window->swap_buffers();

        engine.tick(dt);
      }
    }

  private:

    std::string server;
    int port;

    agario::pid player_pid; // pid of the player we're tracking
    // std::vector<agario::pid> bot_pids;
    std::pair<agario::pid, agario::pid> range_bot_pids;
    agario::Engine<true> engine;

    std::unique_ptr<agario::Renderer> renderer;
    std::shared_ptr<Window> window;

    template <typename T>
    void add_bot(int num_bots) {
      agario::pid pid = 0; 
      for (int i = 0; i < num_bots; i++)
        {
          pid = engine.add_player<T>();

          range_bot_pids.first = std::min(range_bot_pids.first, pid);
          range_bot_pids.second = std::max(range_bot_pids.second, pid);
        }
    }

    void process_input() {
      GLFWwindow *win = window->pointer();
      auto &player = engine.player(player_pid);

      double xpos, ypos;
      glfwGetCursorPos(win, &xpos, &ypos);
      player.target = renderer->to_target(player, xpos, ypos);
      player.action = agario::action::none;

      if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win, true);

      if (glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS)
        player.action = agario::action::split;

      if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)
        player.action = agario::action::feed;
    }

  };

}
