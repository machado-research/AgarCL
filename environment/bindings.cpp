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
  /* =======================GoBigger Environment =======================*/

PYBIND11_MODULE(pyagario, m) {
  m.doc() = "Pybindings for GoBiggerObservation classes in Agario Environment";
  // Bind GlobalState
  py::class_<agario::env::GlobalState>(m, "GlobalState")
      .def(py::init<int, int, int, int, int>(),
            py::arg("width"), py::arg("height"), py::arg("frame_limit"), py::arg("last_frame"), py::arg("team_num"))
      .def("update_last_frame_count", &agario::env::GlobalState::update_last_frame_count)
      .def("get_map_width", &agario::env::GlobalState::get_map_width)
      .def("get_map_height", &agario::env::GlobalState::get_map_height)
      .def("get_frame_limit", &agario::env::GlobalState::get_frame_limit)
      .def("get_team_num", &agario::env::GlobalState::get_team_num);

  // Bind PlayerState
  py::class_<agario::env::PlayerState>(m, "PlayerState")
      .def(py::init<int, const agario::env::vecpii&, const agario::env::vecpii&, 
                    const agario::env::vecpii&, const agario::env::vecpii&, 
                    const std::string&, double, bool, bool>(),
            py::arg("player_id"), py::arg("food_positions"), py::arg("thorn_positions"),
            py::arg("spore_positions"), py::arg("clone_positions"), py::arg("team_name"),
            py::arg("score"), py::arg("can_wject"), py::arg("can_split"))
      .def("get_player_id", &agario::env::PlayerState::get_player_id)
      .def("get_food_positions", &agario::env::PlayerState::get_food_positions)
      .def("get_virus_positions", &agario::env::PlayerState::get_virus_positions)
      .def("get_spore_positions", &agario::env::PlayerState::get_spore_positions)
      .def("get_clone_positions", &agario::env::PlayerState::get_clone_positions)
      .def("get_team_name", &agario::env::PlayerState::get_team_name)
      .def("get_score", &agario::env::PlayerState::get_score)
      .def("canEject", &agario::env::PlayerState::canEject)
      .def("canSplit", &agario::env::PlayerState::canSplit)
      .def("update_score", &agario::env::PlayerState::update_score);

  // Bind PlayerStates
  py::class_<agario::env::PlayerStates>(m, "PlayerStates")
      .def(py::init<const std::unordered_map<int, agario::env::PlayerState>&>(),
         py::arg("player_states"))
      .def("update_player_state", &agario::env::PlayerStates::update_player_state)
      .def("get_all_player_states", &agario::env::PlayerStates::get_all_player_states,
            py::return_value_policy::reference_internal);

  // Bind GoBiggerObservation
  py::class_<agario::env::GoBiggerObservation>(m, "GoBiggerObservation")
      .def(py::init<int, int, int,int, int>(),
            py::arg("map_width"), py::arg("map_height"), py::arg("frame_limit"), py::arg("last_frame"), py::arg("team_num"))
      .def("update_global_state", &agario::env::GoBiggerObservation::update_global_state)
      .def("update_player_state", &agario::env::GoBiggerObservation::update_player_state,
            py::arg("player_id"), py::arg("food_positions"), py::arg("thorn_positions"),
            py::arg("spore_positions"), py::arg("clone_positions"), py::arg("team_name"),
            py::arg("score"), py::arg("can_eject"), py::arg("can_split"))
      .def("get_global_state", &agario::env::GoBiggerObservation::get_global_state,
            py::return_value_policy::reference_internal)
      .def("get_player_states", &agario::env::GoBiggerObservation::get_player_states,
            py::return_value_policy::reference_internal);
}


PYBIND11_MODULE(agarle, module) {
  using namespace py::literals;
  module.doc() = "Agar.io Learning Environment";

  /* ================ Grid Environment ================ */
  using GridEnvironment = agario::env::GridEnvironment<int, renderable>;

  py::class_<GridEnvironment>(module, "GridEnvironment")
    .def(py::init<int, int, int, bool, int, int, int, bool, int>())
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

   .def(pybind11::init<int, int, int, bool, int, int, int, int, int, screen_len, screen_len, bool, bool>())
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

}
