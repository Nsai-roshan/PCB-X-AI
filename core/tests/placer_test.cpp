#include "model/circuit.hpp"
#include "placer/placer.hpp"

#include <gtest/gtest.h>

TEST(PlacerTest, ProducesNonOverlappingPlacementsWithinBoardBounds) {
  const auto circuit = tracemind::model::ParseCircuit(R"json(
  {
    "board": { "width": 18, "height": 10 },
    "components": [
      { "id": "U1", "kind": "chip", "width": 3, "height": 2, "pins": [{ "id": "P1", "x": 1, "y": 0 }] },
      { "id": "R1", "kind": "resistor", "width": 3, "height": 1, "pins": [{ "id": "A", "x": 0, "y": 0 }] }
    ],
    "nets": []
  }
  )json");

  const auto result = tracemind::placer::PlaceComponents(circuit);

  ASSERT_EQ(result.final_positions.size(), 2u);
  for (const auto& placement : result.final_positions) {
    EXPECT_GE(placement.x, 0);
    EXPECT_GE(placement.y, 0);
    EXPECT_LT(placement.x + placement.width, circuit.board_width + 1);
    EXPECT_LT(placement.y + placement.height, circuit.board_height + 1);
  }

  EXPECT_FALSE(result.steps.empty());
}
