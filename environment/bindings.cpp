#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/numpy.h>

#include <tuple>
#include <iostream>
#include <environment/envs/GridEnvironment.hpp>
#include <environment/envs/RamEnvironment.hpp>

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

// update to_vector function to handle different types:
template <typename T>
std::vector<T> to_vector(const std::vector<T> &vec) {
  // std::cout << "To vector" << std::endl;
  return std::vector<T>(vec.begin(), vec.end());
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

  auto &observations = environment.get_observations();\
  py::list obs;
  // for (auto &observation : observations) {
    std::cout << "Num frames: "<< observations.num_frames() << std::endl;
  for (int frame_index = 0; frame_index < observations.num_frames(); ++frame_index) {
    // std::cout << "Processing frame " << frame_index << std::endl;
    // make a copy of the data for the numpy array to take ownership of
    auto *data = new dtype[observations.length()];
    // auto *data = new dtype[observation.length()];

    std::copy(observations.frame_data(frame_index),
              observations.frame_data(frame_index) + observations.length(),
              data);
    // std::copy(observation.data(),
    //           observation.data() + observation.length(),
    //           data);

    // const auto &shape = observations.shape();
    // const auto &strides = observations.strides();
    std::vector<ssize_t> shape = {1024, 1024, 3};
    std::vector<ssize_t> strides = {1024 * 3 * sizeof(dtype), 
                                    3 * sizeof(dtype), 
                                    sizeof(dtype)};
    // const auto &shape = observation.shape();
    // const auto &strides = observation.strides();

    py::capsule cleanup(data, [](void *ptr) {
      auto *data_pointer = reinterpret_cast<dtype*>(ptr);
      delete[] data_pointer;
    });

    // add the numpy array to the list of observations
    obs.append(py::array_t<dtype>(to_vector(shape), to_vector(strides), data, cleanup));
    // std::cout << "obs "<< obs << std::endl;

    // Convert obs to a string and print it
    // if (py::len(obs) > 0) {
    //     py::array first_array = obs.cast<py::array>();
    //     auto shape = first_array.shape();
    //     std::cout << "obs shape: [";
    //     for (ssize_t i = 0; i < first_array.ndim(); ++i) {
    //         std::cout << shape[i];
    //         if (i != first_array.ndim() - 1) {
    //             std::cout << ", ";
    //         }
    //     }
    //     std::cout << "]" << std::endl;
    // } else {
    //     std::cout << "obs is empty" << std::endl;
    // }
    // std::cout << "Done processing observations "<< std::endl;
  }
  // std::cout << "obs[0] size "<< py::len(obs[0]) << std::endl;
  // std::cout << "obs size "<< py::len(obs) << std::endl;

  // Combine the numpy arrays in obs_list into a single numpy array with shape [4, 1024, 1024, 3]
    py::array combined_array = py::array::ensure(obs);
    // Reshape to [1, 4, 1024, 1024, 3]
    combined_array = combined_array.reshape({1, observations.num_frames(), 1024, 1024, 3});

    // Print the shape of the resulting array
    // auto reshaped_shape = combined_array.shape();
    // std::cout << "reshaped array shape: [";
    // for (ssize_t i = 0; i < combined_array.ndim(); ++i) {
    //     std::cout << reshaped_shape[i];
    //     if (i != combined_array.ndim() - 1) {
    //         std::cout << ", ";
    //     }
    // }
    // std::cout << "]" << std::endl;


  return combined_array; // list of numpy arrays
}

PYBIND11_MODULE(agarle, module) {
  using namespace py::literals;
  module.doc() = "Agar.io Learning Environment";

  /* ================ Grid Environment ================ */
  // using GridEnvironment = agario::env::GridEnvironment<int, renderable>;

  // py::class_<GridEnvironment>(module, "GridEnvironment")
  //   .def(py::init<int, int, int, bool, int, int, int>())
  //   .def("seed", &GridEnvironment::seed)
  //   .def("configure_observation", [](GridEnvironment &env, const py::dict &config) {

  //     int num_frames = config.contains("num_frames")      ? config["num_frames"].cast<int>() : 2;
  //     int grid_size  = config.contains("grid_size")       ? config["grid_size"].cast<int>() : DEFAULT_GRID_SIZE;
  //     bool cells     = config.contains("observe_cells")   ? config["observe_cells"].cast<bool>()   : true;
  //     bool others    = config.contains("observe_others")  ? config["observe_others"].cast<bool>()  : true;
  //     bool viruses   = config.contains("observe_viruses") ? config["observe_viruses"].cast<bool>() : true;
  //     bool pellets   = config.contains("observe_pellets") ? config["observe_pellets"].cast<bool>() : true;

  //     env.configure_observation(num_frames, grid_size, cells, others, viruses, pellets);
  //   })
  //   .def("observation_shape", &GridEnvironment::observation_shape)
  //   .def("dones", &GridEnvironment::dones)
  //   .def("take_actions", [](GridEnvironment &env, const py::list &actions) {
  //     env.take_actions(to_action_vector(actions));
  //   })
  //   .def("reset", &GridEnvironment::reset)
  //   .def("render", &GridEnvironment::render)
  //   .def("step", &GridEnvironment::step)
  //   .def("get_state", &get_state<GridEnvironment>)
  //   .def("close", &GridEnvironment::close);
  
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

  // todo: convert ScreenEnvironment to multi-environment

 using ScreenEnvironment = agario::env::ScreenEnvironment<renderable>;

 py::class_<ScreenEnvironment>(module, "ScreenEnvironment")

    .def(py::init([](int num_agents, int frames_per_step, int arena_size, bool pellet_regen,
                         int num_pellets, int num_viruses, int num_bots,
                         screen_len screen_width, screen_len screen_height) {
            return new agario::env::ScreenEnvironment<true>(num_agents, frames_per_step, arena_size, pellet_regen, num_pellets, num_viruses, num_bots, screen_width, screen_height);
        }))
   .def("seed", &ScreenEnvironment::seed)
  //  .def("observation_shape", &ScreenEnvironment::observation_shape)
   .def("dones", &ScreenEnvironment::dones)
   .def("take_actions", [](ScreenEnvironment &env, const py::list &actions) {
     env.take_actions(to_action_vector(actions));
   })
   .def("reset", &ScreenEnvironment::reset)
   .def("render", &ScreenEnvironment::render)
   .def("step", &ScreenEnvironment::step)
   .def("get_state", &get_state<ScreenEnvironment>);
  //  .def("partial_observation", &ScreenEnvironment::partial_observation); // that's for multi-envornment case i guess

  module.attr("has_screen_env") = py::bool_(true);

#else

  module.attr("has_screen_env") = py::bool_(false);

#endif

}
