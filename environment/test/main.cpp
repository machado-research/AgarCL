#include <gtest/gtest.h>

#include <environment/test/grid-env-test.hpp>
#include <environment/test/ram-env-test.hpp>
#include <environment/test/screen-env-test.hpp>

namespace { }

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}