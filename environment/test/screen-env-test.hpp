#pragma once

#include <gtest/gtest.h>
#include <environment/envs/ScreenEnvironment.hpp>

using namespace agario::env;

namespace {

  TEST(ScreenObservationTest, AlphaGridlineRecoveryRunsForExpectedAlphaValues) {
    ScreenObservation observation(1, 1, 3, true);
    std::vector<std::uint8_t> data(observation.length(), 0);

    // Prepare the vertical chain used by post_processing_frame_data:
    // index 3  -> first pixel alpha (gridline marker)
    // index 7  -> second pixel alpha (non-zero, "gridline above")
    // index 11 -> third pixel alpha (target)
    data[3] = 26;
    data[7] = 1;
    data[11] = 255;

    auto *frame = data.data();
    observation.post_processing_frame_data(frame);

    EXPECT_EQ(data[11], 26);
  }

  TEST(ScreenObservationTest, ValuesUpTo230MoveToSemanticChannel) {
    ScreenObservation observation(1, 1, 1, true);
    std::vector<std::uint8_t> data(observation.length(), 0);

    data[0] = 230;

    auto *frame = data.data();
    observation.post_processing_frame_data(frame);

    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[3], 230);
  }

}
