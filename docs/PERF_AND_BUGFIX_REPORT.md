# AgarCL — Bug Fix & Performance Report

**Date:** 2026-08-20
**Branches:** `master` holds two baseline fixes; `perf/rendering-and-engine`
(11 commits) holds everything else, kept separate so results can be compared
against the published-behavior baseline before merging.

---

## 1. Summary

| | |
|---|---|
| Commits | 13 (2 on `master`, 11 on `perf/rendering-and-engine`) |
| Bugs fixed | 25+ across engine, RL interface, and rendering |
| Screen env throughput | 1,527 → ~3,300 steps/s (**~2.2x**) |
| Engine tick | parity (43.2 → 43.1 us median, 10-bot benchmark) |
| Test suites | `test-envs` 10/12 → **12/12**; `test-engine` 35/35; `test-engine-renderable` 35/35 |
| Reproducibility | same seed now produces the same game (was hash-order dependent) |

---

## 2. Fixes on `master`

### 2.1 `fix(build)` — Link tests against system GTest and glm (`1d60186`)

`test-envs` never compiled: missing `glm` on its link line and an undefined
source variable. The `benchmark` submodule also downloaded a second googletest
that shadowed the system one at link time. With this fixed, the C++
environment test suite ran for the first time — and immediately confirmed two
real bugs (see 3.2).

### 2.2 `fix(engine)` — Remove the correct pellets when eaten (`8580d4f`)

`remove_pellets`/`remove_viruses` collected indices during the tick, then
removed them one at a time with swap-and-pop. Each swap permutes the vector,
so after the first removal every remaining index referred to a different
entity:

- ~25% of removals deleted an arbitrary pellet (measured 19–38 wrong
  deletions per burst);
- eaten pellets survived as "ghosts" that could be re-eaten on later ticks,
  inflating eating rates and generating extra work;
- duplicate detections double-credited mass.

**Fix** (same O(1) swap-and-pop heuristic): deduplicate and apply indices in
descending order, plus a per-tick eaten mask so a pellet is credited and
removed at most once. Overhead is O(eaten log eaten) with eaten/tick measured
at p99 = 2 in bot-heavy play; zero-eating A/B benchmarks show no measurable
regression. After the fix, mass gained reconciles exactly with pellets
removed on every trial.

---

## 3. Fixes on `perf/rendering-and-engine`

### 3.1 Rendering performance (`699ac99`, `be43fbc`, `531cdf8`)

Profiling showed ~95% of screen-env step time was rendering + readback;
pellet draw calls alone were 57%.

| commit | change | steps/s (median) |
|---|---|---|
| — | baseline on branch | 1,527 |
| `699ac99` | cache uniform locations at shader link time (was 2 driver string-lookups *per entity per frame*) | 1,651 |
| `be43fbc` | **instanced rendering**: one shared unit-circle per shape class + per-instance (x, y, radius, color) buffer → whole scene in 4 draw calls (was ~2,800 GL calls/frame; per-entity VAO/VBOs were created lazily *inside the render loop*) | 2,321 |
| `531cdf8` | gate per-frame `glGetError`/`eglGetError` (driver sync round-trips) behind `AGARCL_GL_DEBUG` | 3,378 |

Every step was verified by comparing 20 seeded frames against the previous
renderer: **pixel-identical** (max channel delta 0) on both the RGB and the
agent-view (RGBA + post-processing) paths.

### 3.2 Grid environment usable at all (`e37ba00`)

The grid observation type — the library default — had three independent
defects:

1. **All grid observations were zero.** The environment's design is
   standard action repeat: run the engine `ticks_per_step` ticks, then
   observe the final state once. The grid env's copy-into-buffer step,
   however, contained leftover arithmetic from an abandoned frame-stacking
   idea (`tick_index − (ticks_per_step − num_frames)`), which under the
   actual once-per-step call evaluated to −3 and skipped the copy — so the
   env cleared its buffer, simulated the game, never wrote the state, and
   returned the blank buffer. The game underneath (rewards, masses) was
   correct; only the returned image was empty. Caught independently by the
   repo's own `EnvTest.GetState` once the test suite could build.
   **The screen and GoBigger paths were unaffected** — they use the index
   directly with no arithmetic (verified: screen frames pixel-identical
   across this fix) — and all published task configs use `obs_type: screen`.
2. **Python could not construct it**: undefined local variable
   (`UnboundLocalError`), and a 10-vs-11 argument mismatch in the binding.
3. **User settings were silently discarded**: `kwargs | grid_defaults` has
   right-hand precedence, so the defaults overrode every caller value
   (`grid_size`, `num_frames`, `observe_*`). Now `grid_defaults | kwargs`.

Verified from Python: env constructs, honors `grid_size=32`, returns
`(32, 32, 8)` int32 observations inside the declared space, non-zero every
step. `test-envs` went from 10/12 to 12/12.

### 3.3 Headless / context lifetime (`6c77ab2`, `da302e8`)

Critical for the Linux + EGL research deployment:

- **The FBO was never bound.** All rendering and `glReadPixels` went through
  a hidden GLFW window's default framebuffer — formally undefined for
  occluded/headless surfaces. Observation capture now renders into the FBO
  (`RGBA8`, was quantizing `RGB565`), with `GL_PACK_ALIGNMENT=1` fixing a
  heap overflow for widths not divisible by 4.
- **Multiple environments per process crashed.** `~FrameBufferObject` called
  process-global `glfwTerminate()`, invalidating every other environment's
  context — ruling out in-process vectorized envs. GLFW init/terminate is
  now reference-counted; verified with 3 concurrent envs stepped interleaved
  and closed in one process (previously an abort).
- EGL teardown destroyed whatever context was *current* (possibly another
  env's); it now tears down exactly the handles it created, return-checks
  all EGL calls, and requests `EGL_ALPHA_SIZE 8` (a 0-alpha surface returns
  alpha=255 everywhere, breaking agent-view channel masks headlessly).
- GLFW's error callback threw C++ exceptions across C frames (UB → abort at
  teardown); `Shader` was copyable (double `glDeleteProgram`);
  `~BaseEnvironment` was non-virtual.
- **GPU resource leaks**: every eaten cell/food leaked a VAO+VBO forever
  (move-assignment overwrote live handles), and `players_collision` caused
  every player cell's GL buffers to be destroyed and recreated *every tick*.
  Structurally eliminated: entities no longer own GL objects at all.
  Renderable entity sizes dropped: Pellet 144→56 B, Virus 1,920→88 B,
  Cell 728→96 B — a cache win for every engine scan.

### 3.4 Reward / done / termination semantics (`bb136f6`, `49416e6`)

- **`c_death` (death penalty) was dead four ways**: the flag gating it was
  never set; the sign made it a *bonus*; one env discarded the value; the
  observation hooks zeroed it every step. It now works as a per-agent
  penalty: `reward = Δmass − c_death` on death steps. Verified:
  `c_death=100` shifts exactly the death steps (min reward −1 → −101).
  Note: an agent dying at spawn mass loses nothing in diff reward, so
  without `c_death` there is *no* death signal at low mass.
- **Reward↔agent mapping**: rewards were collected in `unordered_map` order;
  with more than one agent, reward *i* could belong to another agent. Now
  indexed by `pids_`.
- **`dones`**: mode-3's win condition was evaluated on the *previous* step's
  state (and from inside a `const` accessor); the mini-game condition
  depended on hash iteration order. Both now evaluated deterministically in
  `update_dones()` on the post-step state.
- **`reset()` after snapshot load**: `is_loading_env_state` was never
  cleared, so after one `load_env_state` every later `reset()` silently
  no-opped.
- **`pellet_regen` argument was ignored** (stored, never read; regeneration
  controlled only by mode). Now: regen requires mode policy AND the caller's
  flag — all shipped mode configs behave exactly as before, but
  `pellet_regen=False` finally works (verified: 400 → 0 pellets when off).
- **Virus top-up underflow**: food-fed viruses can exceed the target;
  the unsigned deficit wrapped to a huge value.
- **Action noise was a no-op**: computed, validated, then discarded due to a
  rebound loop variable. Now actually applied (and reproducible under seed).
- **Time limits reported as termination**: `terminated=True` instead of
  `truncated=True` breaks value bootstrapping at episode boundaries for any
  algorithm that respects the Gymnasium contract. Verified: step limit now
  yields `terminated=False, truncated=True`.

### 3.5 Physics / determinism (`c28651c`, `6fd7032`)

- **`Velocity::direction()`** computed `atan(dx/dy)` — transposed arguments,
  wrong axis — instead of `atan2(dy, dx)`. Only correct on diagonals;
  divided by zero for `dy=0`; disagreed with the `Velocity(angle, speed)`
  constructor it must invert. It fed virus-pop directions (`disrupt()` also
  double-counted the heading) and GoBigger's `CloneInfo.direction`.
- **Dead players had NaN coordinates** (mass-weighted centroid ÷ zero mass),
  observed on every death step; `static_cast<int>(NaN)` is UB. Now (0, 0).
  Verified: 3,000 grid steps with frequent deaths produce zero non-finite
  observation values; 40k-tick stress run NaN-free.
- **`players_collision` rewritten** (detection heuristic unchanged — same
  cells eligible to eat the same cells). The old bookkeeping: moved cells
  out of live vectors then kept mutating the originals; passed the same
  vector as query and gallery to a solver that moves entries out of the
  gallery; trusted unchecked `lower_bound` (credited mass to wrong cells,
  erased unrelated cells); let two predators absorb the same victim's full
  mass (mass created from nothing); erased mid-iteration; and applied
  results in hash order, so **the same seed produced different games across
  runs and standard libraries**. Now: canonical id-ordered application,
  verified lookups, eaten-once constraint, one deferred compaction pass,
  reusable scratch buffers.
- **Cell identity bug (root cause found via the above)**: the renderable
  ball's hand-written move operations did not preserve `Ball::id` — move
  construction minted a *fresh* id, move assignment copied position but not
  id. Every `std::sort` over renderable cells mutated its own sort key
  mid-sort (undefined behavior — this was the actual segfault), and every
  id-based lookup afterwards could hit the wrong cell. Consequence: the
  renderable (Python/training) build played a subtly different game from
  the non-renderable benchmark build. The custom moves existed only to
  transfer GL buffer ownership; entities no longer own GL handles, so all
  special members are defaulted and identity travels with the cell.

**Semantics guarantee for 3.5:** no game rule changed — detection, eat
eligibility, speeds, splitting are untouched. What changed: mass is conserved
at the eat site, victims die exactly once, and outcomes are a function of the
seed. Verified by a determinism probe (3,000 ticks, 8 fighting/splitting
players: identical mass+cell-count traces under the same seed, divergent
under a different seed) at **engine-tick parity** (interleaved A/B: medians
43.2 → 43.1 us/tick).

### 3.6 Big-cell virus miss (owner-approved fix)

`collides_with(virus)` ranges as far as the cell's radius, but the virus
check scanned a fixed 3×3 neighborhood of 25-unit grid buckets — seeing
only ~25 units out. Any cell with radius > 25 (mass ≳ 2,000) could overlap
a virus beyond that and pass through undetected (probe: mass-10,000 cell
overlapping a virus by 4 units — no reaction). Replaced the grid with a
direct scan of the ~10 viruses: exact at every cell size, and faster —
the grid was rebuilt/torn down every tick (1,600 buckets at arena 1000).
Cells with radius ≤ 25 behave identically (verified); the 10%-heavier
interaction rule and consume-vs-disrupt semantics are untouched.
**Behavior note:** large agents now pop on viruses they previously
ghosted through — expect dynamics changes in virus-heavy configs.
Perf: bots benchmark 41.6 vs 42–43 µs/tick; arena 4000 fixed cost
15.8 → 5.5 µs/tick.

### 3.7 Benchmark tooling (`939189c`)

`bench/screen_perf_run.py` (windowed steps/s, cumulative rate, RSS, reward
stats, 1-min load average → CSV) and `bench/screen_perf_plot.py`
(comparison plots). Load is logged so windows contaminated by other processes
are identifiable when comparing runs taken at different times.

---

## 4. Verification of the final tree (all run post-push)

- Full CMake build, all targets: clean, zero errors/warnings-as-errors.
- `test-engine` 35/35, `test-engine-renderable` 35/35, `test-envs` **12/12**.
- Python smoke battery: screen env (50 steps incl. `c_death` path), grid env
  (non-zero observations), 3 concurrent envs in one process, truncation
  semantics — all pass.
- Official `agario-bench` at parity with pre-branch numbers (Tick/30 57.5 vs
  56.2 us on a loaded machine; within noise).
- Rendering-only commits: 20 seeded frames pixel-identical at every step.
  Engine-fix commits intentionally change game outcomes (that is the fix);
  they are verified by invariants (mass reconciliation, determinism,
  NaN-freedom) rather than frame equality.

### Honest caveats

1. **Benchmark noise**: the machine carried load 4–6 during later
   measurements (~15–40% noise floor observed). Direction and magnitude of
   all improvements are solid; exact steps/s figures deserve a quiet-machine
   rerun with the committed tooling.
2. **Linux/EGL**: all changes are core GL 3.3, identical on the GLFW and EGL
   paths, and compile-clean — but runtime validation on Linux is pending
   (opening a PR triggers the Ubuntu CI build + engine tests).
3. **Training curves will shift vs published master** for eating-heavy
   scenarios: ghost-pellet re-eating, double-eat mass duplication and wrong
   virus-pop directions are gone. That is the code becoming correct, but
   expect measurable deltas.

---

## 5. Remaining work

| item | type | note |
|---|---|---|
| B1 engine redesign | perf | persistent pellet grid (pellets never move; stop rebuilding per tick), calibrated bucket size + dynamic scan radius. Virus part done (see below). Top remaining bottleneck. |
| GoBigger cluster | bug+perf | O(n²) deep copies per step (PlayerState by value, committed per entity), unbounded `no_frames` → observation space shape `(0, 512, 512)`, stale `observations` vector, hot-path stderr prints. |
| GIL release around `step()` | perf | zero parallelism for threaded vector envs today. |
| Zero-copy observations | perf | double-buffer to drop the 512 KB/step copy; pass a base handle in `get_frame`. |
| Agent-view post-processing | perf | full CPU pass over W×H×4 bytes per step; belongs in the fragment shader. |
| Non-renderable build | build | `GridEnvironment` unconditionally initializes render-only members. |
| 2M-step master-vs-branch comparison | validation | tooling committed; isolated worktree builds verified byte-identical; note master aborts at interpreter teardown (its own bug) and transiently blocks the *next* process — leave a gap between sequential runs. |

---

## 6. Commit index

```
master:
  1d60186  fix(build):  Link tests against system GTest and glm
  8580d4f  fix(engine): Remove the correct pellets when eaten

perf/rendering-and-engine:
  699ac99  perf(render): Cache uniform locations at shader link time
  6c77ab2  fix(render):  Capture observations from the FBO, not the hidden window
  be43fbc  perf(render): Draw entities with instancing, one call per shape class
  531cdf8  perf(render): Gate per-frame GL error checks behind AGARCL_GL_DEBUG
  da302e8  fix(render):  Remove per-entity GL state and repair context lifetime
  e37ba00  fix(env):     Make grid observations non-zero and constructible
  c28651c  fix(engine):  Correct velocity direction and dead-player centroid
  bb136f6  fix(env):     Repair reward, done and regeneration semantics
  49416e6  fix(env):     Apply action noise, report time limits as truncation
  6fd7032  fix(engine):  Make player-vs-player eating deterministic and conservative
  939189c  feat(bench):  Add throughput benchmark and plot scripts for screen env
```
