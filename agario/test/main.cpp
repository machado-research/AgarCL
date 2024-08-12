#include <gtest/gtest.h>

#include <agario/test/test-core.hpp>
#include <agario/test/test-entities.hpp>
#include <agario/test/test-engine.hpp>

namespace { }

int main(int argc, char *argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
