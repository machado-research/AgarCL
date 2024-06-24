#include <benchmark/benchmark.h>

#include <agario/engine/Engine.hpp>
#include <agario/bots/ExampleBot.hpp>

static void CreateEngine(benchmark::State& state) {
  for (auto _ : state) {
    agario::Engine<false> engine;
    benchmark::DoNotOptimize(engine);
  }
}
BENCHMARK(CreateEngine);

static void Tick(benchmark::State& state) {
  using Bot = agario::bot::ExampleBot<false>;

  agario::Engine<false> engine;
  engine.reset();
  agario::time_delta dt(1.0 / 60);
  int tick_limit = 4 * 3600;

  int num_bots = state.range(0);
  for (int i = 0; i < num_bots; i++)
    engine.add_player<Bot>();

  for (auto _ : state) {
      engine.tick(dt);

    state.PauseTiming();
    if (engine.ticks() > tick_limit) {
      engine.reset();
      for (int i = 0; i < num_bots; i++)
        engine.add_player<Bot>();
    }
    state.ResumeTiming();
  }
}
BENCHMARK(Tick)->Arg(0)->Arg(5)->Arg(10)->Arg(20)->Arg(30);

BENCHMARK_MAIN();
