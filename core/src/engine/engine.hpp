#pragma once

#include "model/circuit.hpp"
#include <nlohmann/json.hpp>

namespace tracemind::engine {

struct RunParams {
  double trace_resistance_per_cell = 0.05;  // ohms per grid cell
  double load_current_per_pin_a    = 0.01;  // amps per load pin
  double violation_threshold_mv    = 30.0;  // mV threshold for IR violations
  double supply_voltage            = 3.3;   // volts
  double mcu_power_w               = 0.15;  // watts per MCU/IC/SoC
  double led_power_w               = 0.05;  // watts per LED
  double default_power_w           = 0.02;  // watts for resistors/caps/etc
};

nlohmann::json BuildRunJson(const model::Circuit& circuit,
                            const RunParams& params = RunParams{});

}  // namespace tracemind::engine