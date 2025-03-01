#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>

#include <tuple>
#include <iostream>
#include <environment/envs/GridEnvironment.hpp>
#include <environment/envs/RamEnvironment.hpp>
#include <environment/envs/GoBiggerEnvironment.hpp>

#ifdef INCLUDE_SCREEN_ENV
#include <environment/envs/ScreenEnvironment.hpp>
#endif

#include <environment/renderable.hpp>

namespace py = pybind11;

template <class Tuple,
  class T = std::decay_t<std::tuple_element_t<0, std::decay_t<Tuple>>>>
std::vector<T> to_vector(Tuple&& tuple) {
  return std::apply([](auto&&... elems) {
    return std::vector<T>{std::forward<decltype(elems)>(elems)...};
  }, std::forward<Tuple>(tuple));
}


py::list get_state_goBigger(const agario::env::GoBiggerEnvironment<renderable>& environment) {
    py::list state_list;
    // For each observation in our environment...
    for (auto &observation : environment.get_observations()) {
        py::dict obs_dict;
        // Put the two “maps” into a dict. These are already bound separately.
        obs_dict["global_state"] = observation.get_global_state();
        obs_dict["player_states"] = observation.get_player_states();
        state_list.append(obs_dict);
    }
    return state_list;
}

/* converts a python list of actions to the C++ action wrapper */
std::vector<agario::env::Action> to_action_vector(const py::list &actions) {
  std::vector<agario::env::Action> acts;
  acts.reserve(actions.size());

  for (auto &action : actions) {
    auto t = py::cast<py::tuple>(action);

    auto dx = py::cast<float>(t[0]);
    auto dy = py::cast<float>(t[1]);
    auto a = static_cast<agario::action>(py::cast<int>(t[2]));

    acts.emplace_back(dx, dy, a);
  }
  return acts;
}

/* extracts observations from each agent, wrapping them in NumPy arrays */
template <typename Environment>
py::list get_state(const Environment &environment) {
  using dtype = typename Environment::dtype;

  auto &observations = environment.get_observations();
  py::list obs;
  for (auto &observation : observations) {
    // make a copy of the data for the numpy array to take ownership of
    auto *data = new dtype[observation.length()];
    std::copy(observation.data(), observation.data() + observation.length(), data);

    const auto &shape = observation.shape();
    const auto &strides = observation.strides();

    py::capsule cleanup(data, [](void *ptr) {
      auto *data_pointer = reinterpret_cast<dtype*>(ptr);
      delete[] data_pointer;
    });

    // add the numpy array to the list of observations
    obs.append(py::array_t<dtype>(to_vector(shape), to_vector(strides), data, cleanup));
  }

  return obs; // list of numpy arrays
}

PYBIND11_MODULE(agarle, module) {
  using namespace py::literals;
  module.doc() = "Agar.io Learning Environment";

  /* ================ Grid Environment ================ */
  using GridEnvironment = agario::env::GridEnvironment<int, renderable>;

  py::class_<GridEnvironment>(module, "GridEnvironment")
    .def(py::init<int, int, int, bool, int, int, int, int, int, int>())
    .def("seed", &GridEnvironment::seed)
    .def("configure_observation", [](GridEnvironment &env, const py::dict &config) {

      int num_frames = config.contains("num_frames")      ? config["num_frames"].cast<int>() : 1;
      int grid_size  = config.contains("grid_size")       ? config["grid_size"].cast<int>() : DEFAULT_GRID_SIZE;
      bool cells     = config.contains("observe_cells")   ? config["observe_cells"].cast<bool>()   : true;
      bool others    = config.contains("observe_others")  ? config["observe_others"].cast<bool>()  : true;
      bool viruses   = config.contains("observe_viruses") ? config["observe_viruses"].cast<bool>() : true;
      bool pellets   = config.contains("observe_pellets") ? config["observe_pellets"].cast<bool>() : true;

      env.configure_observation(num_frames, grid_size, cells, others, viruses, pellets);
    })
    .def("observation_shape", &GridEnvironment::observation_shape)
    .def("dones", &GridEnvironment::dones)
    .def("take_actions", [](GridEnvironment &env, const py::list &actions) {
      env.take_actions(to_action_vector(actions));
    })
    .def("get_frame", []( GridEnvironment &env) {
      auto& observation = env.get_frame();
      auto data = (void *)observation.frame_data();
      auto shape = observation.frame_shape();
      auto strides = observation.frame_strides();
      auto format = py::format_descriptor<std::uint8_t>::format();
      auto buffer = py::buffer_info(data, sizeof(std::uint8_t), format, shape.size(), shape, strides);
      auto arr = py::array_t<std::uint8_t>(buffer);
      return arr;
    })
    .def("reset", &GridEnvironment::reset)
    .def("render", &GridEnvironment::render)
    .def("step", &GridEnvironment::step)
    .def("get_state", &get_state<GridEnvironment>)
    .def("close", &GridEnvironment::close)
    .def("save", &GridEnvironment::save);
  /* ================ Ram Environment ================ */
  // using RamEnvironment = agario::env::RamEnvironment<renderable>;

  // py::class_<RamEnvironment>(module, "RamEnvironment")
  //   .def(py::init<int, int, int, bool, int, int, int>())
  //   .def("seed", &RamEnvironment::seed)
  //   .def("observation_shape", &RamEnvironment::observation_shape)
  //   .def("dones", &RamEnvironment::dones)
  //   .def("take_actions", [](RamEnvironment &env, const py::list &actions) {
  //     env.take_actions(to_action_vector(actions));
  //   })
  //   .def("reset", &RamEnvironment::reset)
  //   .def("render", &RamEnvironment::render)
  //   .def("step", &RamEnvironment::step)
  //   .def("get_state", &get_state<RamEnvironment>);


  /* ================ Screen Environment ================ */
  /* we only include this conditionally if OpenGL was found available for linking */

#ifdef INCLUDE_SCREEN_ENV

 using ScreenEnvironment = agario::env::ScreenEnvironment<renderable>;

 py::class_<ScreenEnvironment>(module, "ScreenEnvironment")

   .def(pybind11::init<int, int, int, bool, int, int, int,bool,int, int, screen_len, screen_len, bool>())
   .def("seed", &ScreenEnvironment::seed)
   .def("observation_shape", &ScreenEnvironment::observation_shape)
   .def("dones", &ScreenEnvironment::dones)
   .def("take_actions", [](ScreenEnvironment &env, const py::list &actions) {
     env.take_actions(to_action_vector(actions));
   })
   .def("reset", &ScreenEnvironment::reset)
   .def("render", &ScreenEnvironment::render)
   .def("step", &ScreenEnvironment::step)
    // .def("get_state", &get_state<ScreenEnvironment>);
    .def("get_state", []( ScreenEnvironment &env) {
      py::list obs;
      auto &observation = env.get_state();
      auto data = (void *) observation.frame_data();
      auto shape = observation.shape();
      auto strides = observation.strides();
      auto format = py::format_descriptor<std::uint8_t>::format();
      auto buffer = py::buffer_info(data, sizeof(std::uint8_t), format, shape.size(), shape, strides);
      auto arr = py::array_t<std::uint8_t>(buffer);
      obs.append(arr);
      return obs;
    })
    .def("close", &ScreenEnvironment::close)
    .def("save", &ScreenEnvironment::save);
  module.attr("has_screen_env") = py::bool_(true);

#else

  module.attr("has_screen_env") = py::bool_(false);

#endif

  /* =======================GoBigger Environment =======================*/
  module.doc() = "Pybindings for GoBiggerObservation classes in Agario Environment";

  // Bind the info structs.
  py::class_<agario::env::FoodInfo>(module, "FoodInfo")
      .def_readwrite("position", &agario::env::FoodInfo::position)
      .def_readwrite("radius", &agario::env::FoodInfo::radius)
      .def_readwrite("score", &agario::env::FoodInfo::score);

  py::class_<agario::env::VirusInfo>(module, "VirusInfo")
      .def_readwrite("position", &agario::env::VirusInfo::position)
      .def_readwrite("radius", &agario::env::VirusInfo::radius)
      .def_readwrite("score", &agario::env::VirusInfo::score)
      .def_readwrite("velocity", &agario::env::VirusInfo::velocity);

  py::class_<agario::env::SporeInfo>(module, "SporeInfo")
      .def_readwrite("position", &agario::env::SporeInfo::position)
      .def_readwrite("radius", &agario::env::SporeInfo::radius)
      .def_readwrite("score", &agario::env::SporeInfo::score)
      .def_readwrite("velocity", &agario::env::SporeInfo::velocity)
      .def_readwrite("owner", &agario::env::SporeInfo::owner);

  py::class_<agario::env::CloneInfo>(module, "CloneInfo")
      .def_readwrite("position", &agario::env::CloneInfo::position)
      .def_readwrite("radius", &agario::env::CloneInfo::radius)
      .def_readwrite("score", &agario::env::CloneInfo::score)
      .def_readwrite("velocity", &agario::env::CloneInfo::velocity)
      .def_readwrite("direction", &agario::env::CloneInfo::direction)
      .def_readwrite("owner", &agario::env::CloneInfo::owner)
      .def_readwrite("teamId", &agario::env::CloneInfo::teamId);


  // Bind GlobalState
  py::class_<agario::env::GlobalState>(module, "GlobalState")
      .def(py::init<int, int, int, int, int>(),
            py::arg("width"), py::arg("height"), py::arg("frame_limit"), py::arg("last_frame"), py::arg("team_num"))
      .def("update_last_frame_count", &agario::env::GlobalState::update_last_frame_count)
      .def("get_map_width", &agario::env::GlobalState::get_map_width)
      .def("get_map_height", &agario::env::GlobalState::get_map_height)
      .def("get_frame_limit", &agario::env::GlobalState::get_frame_limit)
      .def("get_team_num", &agario::env::GlobalState::get_team_num);

  // Bind PlayerStats
  py::class_<agario::env::PlayerState>(module, "PlayerState")
    .def(py::init<int,
                  const std::vector<agario::env::FoodInfo>&,
                  const std::vector<agario::env::VirusInfo>&,
                  const std::vector<agario::env::SporeInfo>&,
                  const std::vector<agario::env::CloneInfo>&,
                  const std::string&, double, bool, bool>(),
         py::arg("player_id"),
         py::arg("food_infos"),
         py::arg("virus_infos"),
         py::arg("spore_infos"),
         py::arg("clone_infos"),
         py::arg("team_name"),
         py::arg("score"),
         py::arg("can_eject"),
         py::arg("can_split"))
    .def("get_player_id", &agario::env::PlayerState::get_player_id)
    .def("get_food_infos", &agario::env::PlayerState::get_food_infos,
         py::return_value_policy::reference_internal)
    .def("get_virus_infos", &agario::env::PlayerState::get_virus_infos,
         py::return_value_policy::reference_internal)
    .def("get_spore_infos", &agario::env::PlayerState::get_spore_infos,
         py::return_value_policy::reference_internal)
    .def("get_clone_infos", &agario::env::PlayerState::get_clone_infos,
         py::return_value_policy::reference_internal)
    .def("get_team_name", &agario::env::PlayerState::get_team_name)
    .def("get_score", &agario::env::PlayerState::get_score)
    .def("canEject", &agario::env::PlayerState::canEject)
    .def("canSplit", &agario::env::PlayerState::canSplit)
    .def("update_score", &agario::env::PlayerState::update_score);

  // Bind PlayerStates
  py::class_<agario::env::PlayerStates>(module, "PlayerStates")
      .def(py::init<const std::unordered_map<int, agario::env::PlayerState>&>(),
         py::arg("player_states"))
      .def("update_player_state", &agario::env::PlayerStates::update_player_state)
      .def("get_player_state", &agario::env::PlayerStates::get_player_state)
      .def("get_all_player_states", &agario::env::PlayerStates::get_all_player_states,
            py::return_value_policy::reference_internal);

  // Bind GoBiggerObservation
  using GoBiggerObservation = agario::env::GoBiggerObservation<renderable>;
  py::class_<GoBiggerObservation>(module, "GoBiggerObservation")
      .def(py::init<int, int, int,int, int>(),
            py::arg("map_width"), py::arg("map_height"), py::arg("frame_limit"), py::arg("last_frame"), py::arg("team_num"))
      .def("update_global_state", &GoBiggerObservation::update_global_state)
      .def("update_player_state", &GoBiggerObservation::update_player_state,
          py::arg("player_id"),
          py::arg("food_infos"),
          py::arg("virus_infos"),
          py::arg("spore_infos"),
          py::arg("clone_infos"),
          py::arg("team_name"),
          py::arg("score"),
          py::arg("can_eject"),
          py::arg("can_split"))
      .def("update_player_state", &GoBiggerObservation::update_player_state,
            py::arg("player_id"), py::arg("food_positions"), py::arg("thorn_positions"),
            py::arg("spore_positions"), py::arg("clone_positions"), py::arg("team_name"),
            py::arg("score"), py::arg("can_eject"), py::arg("can_split"))
      .def("get_global_state", &GoBiggerObservation::get_global_state,
            py::return_value_policy::reference_internal)
      .def("get_player_states", &GoBiggerObservation::get_player_states,
            py::return_value_policy::reference_internal);

  // Bind GoBiggerEnvironment
  using GoBiggerEnv = agario::env::GoBiggerEnvironment<renderable>;

  py::class_<GoBiggerEnv>(module, "GoBiggerEnvironment")
      .def(py::init<int, int, int, int, int, int, bool, int, int, int, bool, int, int, bool>(),
          py::arg("map_width"),
          py::arg("map_height"),
          py::arg("frame_limit"),
          py::arg("num_agents"),
          py::arg("ticks_per_step"),
          py::arg("arena_size"),
          py::arg("pellet_regen"),
          py::arg("num_pellets"),
          py::arg("num_viruses"),
          py::arg("num_bots"),
          py::arg("reward_type"),
          py::arg("c_death") = 0,
          py::arg("mode_number") = 0,
          py::arg("agent_view") = false)

      .def("configure_observation", [](GoBiggerEnv &env, const py::dict &config) {

      int num_frames = config.contains("num_frames")      ? config["num_frames"].cast<int>() : 1;
      int grid_size  = config.contains("grid_size")       ? config["grid_size"].cast<int>() : DEFAULT_GRID_SIZE;
      bool cells     = config.contains("observe_cells")   ? config["observe_cells"].cast<bool>()   : true;
      bool others    = config.contains("observe_others")  ? config["observe_others"].cast<bool>()  : true;
      bool viruses   = config.contains("observe_viruses") ? config["observe_viruses"].cast<bool>() : true;
      bool pellets   = config.contains("observe_pellets") ? config["observe_pellets"].cast<bool>() : true;

      env.configure_observation(num_frames, grid_size, cells, others, viruses, pellets);
    })
      // Bind additional methods as needed.
      .def("get_state", &get_state_goBigger)
      .def("get_frame", []( GoBiggerEnv &env) {
        auto& observation = env.get_frame();
        auto data = (void *)observation.frame_data();
        auto shape = observation.frame_shape();
        auto strides = observation.frame_strides();
        auto format = py::format_descriptor<std::uint8_t>::format();
        auto buffer = py::buffer_info(data, sizeof(std::uint8_t), format, shape.size(), shape, strides);
        auto arr = py::array_t<std::uint8_t>(buffer);
        return arr;
      })
      .def("take_actions", [](GoBiggerEnv &env, const py::list &actions) {
        env.take_actions(to_action_vector(actions));
      })
      .def("dones", &GoBiggerEnv::dones)
      .def("observation_shape", &GoBiggerEnv::observation_shape)
      .def("seed", &GoBiggerEnv::seed, "Seed the environment")
      .def("reset", &GoBiggerEnv::reset, "Reset the environment")
      .def("step", &GoBiggerEnv::step, "Step through the environment")
      .def("render", &GoBiggerEnv::render, "Render the current state")
      .def("close", &GoBiggerEnv::close, "Close the environment")
      .def("save", &GoBiggerEnv::save, "Save the current state");
}
