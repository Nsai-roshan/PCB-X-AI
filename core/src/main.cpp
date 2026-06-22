#include "engine/engine.hpp"
#include "model/circuit.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: tracemind_engine <circuit-json-path> [params-json]\n";
    return 1;
  }

  std::ifstream input(argv[1]);
  if (!input) {
    std::cerr << "Failed to open circuit file: " << argv[1] << "\n";
    return 1;
  }

  std::stringstream buffer;
  buffer << input.rdbuf();

  // Optional second arg: JSON object with parameter overrides
  tracemind::engine::RunParams params;
  if (argc >= 3) {
    try {
      const auto pj = nlohmann::json::parse(argv[2]);
      if (pj.contains("traceResistancePerCell"))
        params.trace_resistance_per_cell = pj["traceResistancePerCell"].get<double>();
      if (pj.contains("loadCurrentPerPinA"))
        params.load_current_per_pin_a = pj["loadCurrentPerPinA"].get<double>();
      if (pj.contains("violationThresholdMv"))
        params.violation_threshold_mv = pj["violationThresholdMv"].get<double>();
      if (pj.contains("supplyVoltage"))
        params.supply_voltage = pj["supplyVoltage"].get<double>();
      if (pj.contains("mcuPowerW"))
        params.mcu_power_w = pj["mcuPowerW"].get<double>();
      if (pj.contains("ledPowerW"))
        params.led_power_w = pj["ledPowerW"].get<double>();
      if (pj.contains("defaultPowerW"))
        params.default_power_w = pj["defaultPowerW"].get<double>();
    } catch (...) {
      // ignore malformed params, use defaults
    }
  }

  try {
    const auto circuit = tracemind::model::ParseCircuit(buffer.str());
    const auto payload = tracemind::engine::BuildRunJson(circuit, params);
    std::cout << payload.dump();
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }

  return 0;
}