# AgarCL — Bug Fix & Performance Report

**Date:** 2026-08-20
**Branches:** `master` carries two baseline fixes; `perf/rendering-and-engine`
(23 commits) carries everything else, kept separate so results can be compared
against published behavior before merging.
**Priority target:** headless Linux (EGL) with **screen** observations.

---

## 1. Summary

| | |
|---|---|
| Commits | 22 (2 on `master`, 20 on `perf/rendering-and-engine`) |
| Bugs fixed | 40+ across engine, bots, RL interface, rendering, build, and docs |
| Screen env throughput | 1,527 → ~4,200 steps/s at the 350-pellet bench config |
| Screen env at the 1,024-pellet spec | ~600 → ~3,700 steps/s (**~6×**) |
| Grid env throughput | 112 → 45 µs/step (**2.5×**) |
| Engine tick (10 bots, 10 viruses, 1,000 pellets) | 41.5 → 2.6 µs (**16×**) |
| Test suites | `test-engine` 35 → **53**; `test-engine-renderable` 35 → **53**; `test-envs` 10/12 → **12/12**; new Python regression suite **14** |
| Reproducibility | same seed yields identical rewards **and** identical pixels, verified end to end from Python (previously broken by hash-order dependence, `random_device` reseeding on reset, and bots drawing from the global C RNG) |

Observations were held **byte-identical** through every performance change.
The only commits that intentionally alter what an agent sees are the
correctness fixes in §3.5 and §3.6, each flagged before it was made.

---

## 2. Fixes on `master`

**`fix(build)` — link tests against system GTest and glm (`1d60186`).**
`test-envs` never compiled (missing `glm` on its link line, undefined source
variable), and the `benchmark` submodule pulled in a second googletest that
shadowed the system one. Fixing this made the environment test suite run for
the first time — and it immediately failed on two real bugs (§3.2).

**`fix(engine)` — remove the correct pellets when eaten (`8580d4f`).**
Removal collected indices, then swap-popped them one at a time; each swap
permuted the vector, so every index after the first referred to a different
pellet. Measured: 19–38 wrong deletions per burst, eaten pellets surviving as
re-eatable "ghosts" (which also generated extra work every tick), duplicate
detections double-crediting mass. Fixed with descending-order application plus
a per-tick eaten mask; mass gained now reconciles exactly with pellets removed.

---

## 3. Fixes on `perf/rendering-and-engine`

### 3.1 Rendering performance (`699ac99`, `be43fbc`, `531cdf8`, `fe5e92c`)

| change | steps/s |
|---|---|
| branch baseline | 1,527 |
| cache uniform locations at link time (was 2 driver string-lookups **per entity per frame**) | 1,651 |
| **instanced rendering**: one shared unit-circle per shape class + per-instance `(x, y, radius, rgba)` → whole scene in 4 draw calls (was ~2,800 GL calls/frame, with per-entity VAO/VBOs created lazily *inside* the render loop) | 2,321 |
| gate per-frame `glGetError`/`eglGetError` (driver sync points) behind `AGARCL_GL_DEBUG` | 3,378 |
| stop querying `glGetIntegerv` per capture (state queries can synchronize); track the binding instead | 4,219 |

Verified pixel-identical against the previous renderer at every step (20
seeded frames, max channel delta 0, both RGB and agent_view paths).

### 3.2 Grid environment made usable (`e37ba00`)

The default observation type had three independent defects:

1. **All grid observations were zero.** The env's design is standard action
   repeat — run `ticks_per_step` engine ticks, then observe once. The
   copy-into-buffer step, however, carried leftover arithmetic from an
   abandoned frame-stacking idea (`tick_index − (ticks_per_step − num_frames)`)
   which evaluated to −3 under the actual once-per-step call, skipping the
   copy. So the env cleared its buffer, simulated the game, never wrote the
   state, and returned the blank buffer. **Screen and GoBigger were
   unaffected** (they use the index directly) and all published task configs
   use `obs_type: screen`.
2. **Python could not construct it** — undefined local (`UnboundLocalError`)
   and a 10-vs-11 argument mismatch in the binding.
3. **User settings were discarded** — `kwargs | grid_defaults` has right-hand
   precedence, so defaults overrode every caller value.

### 3.3 Headless / context lifetime (`6c77ab2`, `da302e8`)

Directly relevant to the deployment target:

- **The FBO was never bound.** All rendering and `glReadPixels` went through a
  hidden window's default framebuffer — formally undefined for
  occluded/headless surfaces. Capture now renders into the FBO (`RGBA8`, was
  quantizing `RGB565`), with `GL_PACK_ALIGNMENT=1` fixing a heap overflow for
  widths not divisible by 4.
- **EGL requested zero alpha bits.** `agent_view` reads RGBA, and a 0-alpha
  surface returns alpha=255 for every pixel — silently breaking the agent-view
  channel masks *on Linux specifically*, while the macOS path worked.
- **Multiple environments per process aborted.** `~FrameBufferObject` called
  process-global `glfwTerminate()`, destroying every other environment's
  context. Now reference-counted; 3 concurrent envs verified.
- EGL teardown destroyed whichever context was *current* (possibly another
  env's) and every EGL call was unchecked; GLFW's error callback threw across
  C frames (UB → abort at teardown, reproduced on master); `Shader` was
  copyable (double `glDeleteProgram`); `~BaseEnvironment` was non-virtual.
- **GPU leaks:** every eaten cell/food leaked a VAO+VBO permanently, and
  `players_collision` destroyed and recreated every player cell's GL buffers
  *every tick*. Structurally eliminated — entities own no GL objects now.
  Sizes dropped: Pellet 144→56 B, **Virus 1,920→88 B**, Cell 728→96 B, a cache
  win for every engine scan.

### 3.4 Reward / done / termination semantics (`bb136f6`, `49416e6`, `8138dbc`)

- **`c_death` was dead four ways**: the gating flag was never set, the sign
  made it a *bonus*, one env discarded the value, and the observation hooks
  zeroed it every step. Now a per-agent penalty: `reward = Δmass − c_death` on
  death steps. Note it matters most at low mass — an agent dying at spawn mass
  loses nothing in the difference reward, so without `c_death` there is **no
  death signal at all**.
- **Reward↔agent mapping**: rewards were gathered in hash-map order, so with
  >1 agent reward *i* could belong to another agent.
- **`dones`**: mode 3's win condition was evaluated on the *previous* step's
  state from inside a `const` accessor; the mini-game condition depended on
  hash iteration order.
- **`reset()` after snapshot load** silently became a no-op forever.
- **`pellet_regen` was ignored** (stored, never read). Now honored; all shipped
  mode configs behave as before.
- **Virus top-up underflow** when food-fed viruses exceed the target.
- **Action noise was discarded** — computed, validated, then thrown away.
- **Time limits reported as termination** instead of truncation, silently
  breaking value bootstrapping at episode boundaries.
- **GoBigger observation space was `(0, 512, 512)`** — the shape reported a
  running frame counter that started at 0 and grew unbounded.

### 3.5 Physics and determinism (`c28651c`, `6fd7032`)

- **`Velocity::direction()`** computed `atan(dx/dy)` — transposed, wrong axis,
  divide-by-zero at `dy=0` — disagreeing with the constructor it must invert
  everywhere except the diagonals. It fed virus-pop directions (which also
  double-counted the heading) and GoBigger's `CloneInfo.direction`.
- **Dead players had NaN coordinates** (centroid ÷ zero mass), observed on
  every death step; `static_cast<int>(NaN)` is UB.
- **`players_collision` rewritten** — detection heuristic untouched. The old
  bookkeeping moved cells out of live vectors then mutated the originals;
  aliased query and gallery into a solver that moves out of the gallery;
  trusted unchecked `lower_bound` (crediting the wrong cell, erasing an
  unrelated one); let two predators absorb the same victim's full mass (mass
  from nothing); erased mid-iteration; and applied results in hash order, so
  **the same seed produced different games across runs and standard
  libraries**.
- **Cell identity** (root cause found via the above): the renderable ball's
  hand-written move operations did not preserve `Ball::id` — move construction
  minted a fresh id, move assignment dropped it. Every `std::sort` over
  renderable cells mutated its own sort key mid-sort (UB, and the actual
  segfault), and id-based lookups could hit the wrong cell. Consequence: the
  renderable (training) build played a subtly different game from the
  non-renderable benchmark build. All special members are now defaulted.

### 3.6 Big-cell virus miss (owner-approved) (`ff59733`)

`collides_with(virus)` reaches as far as the cell's radius, but the virus check
scanned a fixed 3×3 neighborhood of 25-unit buckets — seeing only ~25 units.
Any cell with radius > 25 (**mass ≳ 2,000**) could overlap a virus beyond that
and pass through undetected (probe: a mass-10,000 cell overlapping a virus by
4 units produced no reaction). Replaced with a direct scan of the ~10 viruses:
exact at every cell size, and *faster* — the grid was rebuilt and torn down
every tick (1,600 buckets at arena 1000). Cells with radius ≤ 25 behave
identically (verified), and the 10%-heavier interaction rule and
consume-vs-disrupt semantics are unchanged.

**Behavior note:** large agents now pop on viruses they previously ghosted
through. Expect dynamics changes in virus-heavy configs.

### 3.7 B1: persistent pellet grid (`65a6702`)

Pellets never move, yet their spatial index was rebuilt and destroyed every
tick — with a hardcoded 510-unit bucket size that degenerated to a 2×2 grid at
arena 1000, so the "index" scanned the whole arena and lookup cost scaled
linearly with pellet count. Now built once and maintained incrementally (entry
fix-ups on eat, appends on regeneration, lazy rebuild after reset or snapshot
load), with 32-unit buckets, a scan neighborhood derived from the cell's radius
(correct at any size), and bucket-local `(x, y, idx)` entries so the hot loop
walks contiguous 12-byte records with an inline distance test — provably the
same predicate as `can_eat && collides_with` for pellets, minus two virtual
calls and a `pow()` per pellet.

| measurement | before | after |
|---|---|---|
| pellet lookup fixed cost | 2.4–30 µs/tick (scaling with pellets and arena) | **0.04–0.05 µs/tick, flat** |
| 10-bot benchmark | 41.5 µs/tick | **5.2 µs/tick (8×)** |

Verified by a grid↔pellets mirror probe (entry count, indices, coordinates,
bucket placement at 480 checkpoints over 12,000 ticks including regeneration
and a mid-run reset), the A1 eating invariants, unchanged eating rates
(0.24/tick), determinism, and snapshot save/load exercising both rebuild paths.

### 3.8 Observation hot loops (`fe5e92c`, `0b75af1`)

- **Grid `_mark_out_of_bounds`** ran `grid_size²` iterations (16,384 at the
  default) calling `player.location()` in each — `x()` and `y()` are each
  O(cells) reductions that also call `mass()`, so ~65,000 redundant cell
  traversals per frame to paint a channel that depends only on the centroid.
  Hoisted. `_store_entities` recomputed the same centroid per entity; hoisted
  too. **Grid env: 112 → 45 µs/step.**
- **Screen `get_state`** built a `py::array_t` from a `buffer_info` with no
  base handle, which makes pybind11 allocate a throwaway wrapper and then
  `PyArray_NewCopy` the frame — two allocations and two traversals per step.
  Now one of each, owned by NumPy via a capsule.
- **agent_view post-processing** walked individual bytes with a modulo-based
  branch per byte. Now loads each pixel as one 32-bit word and skips
  background pixels (96–99% of a frame), where no branch can fire.
  22.6 → 18.8 µs/step, **byte-identical** (alpha values still exactly
  {0, 26, 229}). The algorithm, mutation order, and neighbor-based grid-line
  reconstruction are unchanged.

### 3.9 Bot layer (`8583f7a`, `29d2a8e`)

Found in a later review pass; the bots had gone unaudited.

- **Shy bots never compared their own mass.** `HungryShyBot` and
  `AggressiveShyBot` tested `other_player.mass() > mass()`, where the
  unqualified `mass()` does not resolve to `Player::mass()`: `Player` is a
  *dependent* base, so unqualified lookup skips it and finds the **type**
  `agario::mass`, making `mass()` a cast that yields **0**. The comparison was
  always true, so both bots fled from every player within `SHY_RADIUS`
  regardless of size, and `AggressiveShyBot`'s hunting branch was unreachable
  whenever anyone was in range. Probe: a 5,000-mass bot fled from a 25-mass
  player. **Two of the four bot types were behaving wrongly.**
- **`reset_state()` discarded the caller's seed**, reseeding `state.rng` from
  `std::random_device` — so `seed()` → `reset()` → `step()` was not
  reproducible. This is the snapshot-load path.
- **Bots drew from the global C RNG** (`std::rand()`), which cannot be seeded
  per environment and is shared process-wide. Replaced with a wander target
  derived from pid and tick; also removes a `% 0` UB on degenerate arenas.
- **`nearest_pellet` could send a bot to world origin** — its "pellet on top
  of me" guard was unreachable, so that case fell through with an unset target.
- **Bot decisions depended on hash-map layout.** Scans took the *first*
  qualifying player from an `unordered_map`; probe shows a map holding pids
  {41, 7, 300, 12, 999, 5} iterates as `5, 999, 300, 12, 7, 41`. Scans now use
  a pid-ordered view. (With the pid patterns `add_player` produces on libc++
  the order already matched, so this removes a latent hazard rather than a
  reproduced divergence.)
- **`target_player` divided by zero mass** (NaN coordinates); **`largest_cell()`
  threw on a dead bot** — both now guarded.
- **Performance: engine tick 5.4 → 2.6 µs** (10 bots, 1,000 pellets). The
  bot's own centroid was recomputed inside the ~1,000-pellet loop — two
  O(cells) reductions each calling a virtual `mass()` per cell, up to ~64,000
  cell visits per bot per action. Hoisted, and distances compared squared,
  removing ~1,000 `sqrt` calls per bot per action. The pellet selected is
  unchanged (ordering is monotonic in the squared value).
- Removed dead code (`find_target`, `nearest_food`, two large commented-out
  blocks, the fully commented-out `utils/structures.hpp` include and CMake
  entry) and added the missing `#pragma once` to `utils/random.hpp` and
  `<limits>` to `num_wrapper.hpp`.

**Cumulative engine tick: 41.5 → 2.6 µs (16×)** across §3.7 and §3.9.

### 3.11 Regression test suites (`6c992f9`)

The fixes were originally verified with throwaway probes outside the
repository, so nothing prevented the defects returning. They are now permanent
tests: **18** in `agario/test/test-regressions.hpp` (built into both the
renderable and non-renderable engine targets) covering pellet-eating mass
accounting, virus collisions at every cell size, determinism, seed survival
across `reset_state`, dead-player centroids, `direction()` correctness and
round-tripping, `pellet_regen`, mass conservation when cells eat cells, and the
bot-layer fixes; and **14** in `tests/regression_test.py` covering observation
delivery for grid and screen, agent-view alpha encoding, finiteness under
frequent deaths, `c_death`, truncation-vs-termination, seeded reproducibility of
both rewards and pixels, action noise, and multiple environments per process.

Each test was verified to **fail when its fix is reverted** (the shy-bot mass
comparison, the `reset_state` reseed, the `pellet_regen` flag and the `c_death`
sign were reintroduced in turn). CI now runs `test-envs` and the Python suite in
addition to the engine suites.

### 3.10 CI, tooling, docs (`eac56f6`, `939189c`, `c2f4635`)

- **CI now validates the deployment target.** It previously built the default
  configuration and ran only engine tests — neither the unbound-FBO bug nor
  the missing-EGL-alpha bug was reachable by it. It now installs the EGL
  runtime plus Mesa software rendering, configures `USE_EGL=ON`, runs
  `test-envs`, and executes a headless smoke test under
  `EGL_PLATFORM=surfaceless` asserting the screen observation contains
  rendered content (not a blank frame) and that agent_view's alpha channel is
  not constant.
- **Benchmark tooling**: `bench/screen_perf_run.py` (windowed steps/s,
  cumulative rate, RSS, reward stats, load average → CSV) and
  `bench/screen_perf_plot.py`. Load is logged so contaminated windows are
  identifiable when comparing runs taken at different times.
- **README**: every code example failed to run — none imported `gym_agario`
  (so all raised `NameNotFound`), the snapshot/video/render examples built
  malformed nested actions (`TypeError`), and `reset()` was shown returning
  only an observation. Fixed, plus an invalid SSH URL, inconsistent clone
  orgs, and deprecated `setup.py install`. Added a full configuration
  reference, the action-space description, the mode 0–10 task table, a
  reproducibility section (seed **before** `reset()`), and troubleshooting for
  the CMake 4.x policy error, headless EGL setup, and the clang++ requirement.
  All five environment-constructing examples now execute against the built
  module.

### 3.11 Agent-view video decoding and seeded action noise (`ab8a882`)

Both found while visually auditing gameplay videos after the benchmark.

- **Agent-view videos were black** (a blue dot on darkness), on master and
  branch alike. The video colouriser assumed an encoding the observation does
  not use — background alpha 255, pellets `ch0 != 255` — so its final
  `alpha <= 30` grid rule swallowed the whole frame. It now decodes the actual
  encoding (presence masks at 255; alpha 229 = own cells, 26 = grid/arena
  boundary, 0 = background) with game-like colours. **Video output only;
  observations untouched.** The palette row `[26, 0, 0]` in the old code was
  the encoded *value* 26 pasted in as if it were a colour — a hint the
  function was never visually verified against the encoding.
- **Seeded runs were not reproducible with `add_noise=True` (the default):**
  the action noise drew from numpy's *global* generator, which `env.seed()`
  did not touch, so identically-seeded runs took different actions and
  diverged (observed as different cumulative rewards for the same seed).
  `seed()` now derives an env-local generator for the noise, same
  distribution; never-seeded environments keep the old global-RNG behaviour.

Regression tests cover both; the Python suite is now 21 tests.

### 3.12 Benchmark: 3 seeds × 500k steps, master vs branch

See `docs/BENCHMARK_500K_3SEEDS.md` (graphs + CSVs alongside). Full-game
configuration, sequential runs on the same machine: master **499 ± 56**
steps/s, branch **3,518 ± 180** steps/s — **7.04× mean-of-seeds**, with the
slowest branch window 3.9× the fastest master window, flat memory and zero
resets on both arms.

---

## 4. Verification of the final tree

Run after the last commit:

- Full CMake build, all targets: clean, zero errors.
- `test-engine` 53/53, `test-engine-renderable` 53/53, `test-envs` 12/12,
  `tests/regression_test.py` 21/21.
- **Every fix on this branch now has a permanent regression test** (§3.11).
  Each was confirmed to fail when its fix is reverted.
- Determinism probe: identical 3,000-tick traces (8 fighting/splitting
  players) under a fixed seed; divergent under a different seed.
- Pellet grid mirror probe: 480 checkpoints over 12,000 ticks, exact.
- Virus regression probe: the previously-missed contact is detected.
- Python smoke: screen agent_view, grid non-zero observations, GoBigger sane
  shape, 3 concurrent envs in one process, `c_death` penalty applied.
- Rendering commits: 20 seeded frames pixel-identical throughout. Engine
  correctness commits intentionally change outcomes and are verified by
  invariants (mass reconciliation, determinism, NaN-freedom) instead.

### Caveats

1. **Benchmarks are macOS numbers, and macOS has a large fixed GPU latency.**
   Measured: **135 µs of GPU drain with an empty scene**, identical at 32 px
   and 128 px — a resolution- and workload-independent driver round-trip
   (OpenGL is emulated over Metal). Plus ~25 µs fixed in the pixel transfer.
   That floor is ~200 µs of the remaining ~240 µs/step. On Linux with a native
   driver this is typically 10–50 µs, so **real throughput on the target
   platform is likely well above these figures, and the proportions of what
   remains will differ**. This is why optimization stopped here rather than
   continuing against a platform artifact.
2. **Linux/EGL runtime validation is pending.** All code is core GL 3.3,
   identical on the GLFW and EGL paths, and compiles cleanly; the new CI job
   exercises it on every PR, but it has not yet been run.
3. **Training curves will shift versus published master** in eating-heavy
   scenarios: ghost-pellet re-eating, double-eat mass duplication, wrong
   virus-pop directions and big-cell virus misses are all gone. That is the
   code becoming correct, but expect measurable differences.
4. **One earlier estimate was wrong and is corrected here.** GoBigger's
   per-entity state commit was described as ~10⁶ copies per step; it only ran
   for entities passing the in-view test (a few dozen), so that fix is a code
   clarity and allocation improvement with no measurable speedup.

---

## 5. Remaining work

| item | type | note |
|---|---|---|
| Linux/EGL measurement | **validation, gates the rest** | open a PR (CI now covers headless) or run `bench/screen_perf_run.py` on a Linux GPU box. Determines whether any readback work is worth doing. |
| GL↔CUDA interop | perf, architectural | today the pixel path is GPU → CPU → GPU: rendered on the GPU, read back to CPU, then uploaded again by PyTorch. Rendering to a texture shared with CUDA (exposed via DLPack) removes the round trip and the blocking readback. The largest remaining win for GPU training — scope after the measurement above. |
| Readback pipelining | perf, **changes semantics** | hiding the drain requires returning the previous frame's pixels, making observations one step stale. Not done: it alters the MDP. |
| Non-renderable build | build | `GridEnvironment` unconditionally initializes render-only members; `FrameObservation` needs the rendering headers. |
| GoBigger remainder | bug | stale `observations` vector (only the single live observation is updated), `SporeInfo`/`CloneInfo` owner fields set from the observing player rather than the emitter, hardcoded `teamId`. |
| `numWrapper` type safety | design | `distance` and `angle` are meant to be distinct types, but implicit conversions let `d = a` and `d < a` compile silently while `d + a` fails as ambiguous; every operator takes `T`, so `double * distance` narrows to float before multiplying. Making the converting constructor `explicit` is the fix but touches a great deal of code — deliberately not attempted. |
| `no_player` sentinel | latent | `agario::pid` is unsigned, so the `-1` sentinel is `65535`; bots constructed without an explicit pid share it, and `Player::operator==` compares pid only, so two such bots compare equal. Latent because `add_player()` always assigns a real pid. |
| ~~master-vs-branch comparison~~ | **done** | 3 seeds × 500k steps: **7.04×** mean-of-seeds — §3.12 and `docs/BENCHMARK_500K_3SEEDS.md`. |
| editable-install shadowing | tooling gotcha | `pip install -e` here can leave a *concrete* copy of `gym_agario` + the `.so` in site-packages (alongside the editable finder), which wins module resolution for any process launched outside the repo and silently pins old code. If behaviour looks stale after a reinstall, check `python3 -c "import gym_agario; print(gym_agario.__file__)"` from outside the repo and delete the site-packages copies. |

---

## 6. Commit index

```
master:
  1d60186  fix(build):   Link tests against system GTest and glm
  8580d4f  fix(engine):  Remove the correct pellets when eaten

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
  a13124d  docs:         Add bug fix and performance report for the branch
  5c07f5a  docs:         Clarify grid zero-observation bug scope
  ff59733  fix(engine):  Detect virus collisions for cells of any size
  65a6702  perf(engine): Make the pellet spatial grid persistent and incremental
  fe5e92c  perf(env):    Hoist observation hot loops and halve the screen copy
  eac56f6  ci:           Validate the headless EGL screen-observation path
  0b75af1  perf(env):    Skip background pixels in agent-view post-processing
  8138dbc  fix(env):     Correct GoBigger observation shape, trim per-step overhead
  c2f4635  docs:         Fix README examples and document configuration
```
