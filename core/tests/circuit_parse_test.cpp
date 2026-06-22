#include "model/circuit.hpp"

#include <gtest/gtest.h>

TEST(CircuitParseTest, ParsesBasicCircuitJson) {
  const auto circuit = tracemind::model::ParseCircuit(R"json(
  {
    "board": { "width": 20, "height": 12 },
    "components": [
      {
        "id": "BAT1",
        "kind": "battery",
        "width": 3,
        "height": 2,
        "pins": [{ "id": "POS", "x": 2, "y": 0 }, { "id": "NEG", "x": 0, "y": 0 }]
      }
    ],
    "nets": [
      { "id": "VCC", "connections": [{ "component": "BAT1", "pin": "POS" }] }
    ]
  }
  )json");

  EXPECT_EQ(circuit.board_width, 20);
  EXPECT_EQ(circuit.board_height, 12);
  ASSERT_EQ(circuit.components.size(), 1u);
  EXPECT_EQ(circuit.components[0].id, "BAT1");
  ASSERT_EQ(circuit.nets.size(), 1u);
  EXPECT_EQ(circuit.nets[0].id, "VCC");
}
