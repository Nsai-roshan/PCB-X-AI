#include "model/circuit.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace tracemind::model {
namespace {
using json = nlohmann::json;
}

Circuit ParseCircuit(const std::string& json_text) {
  const auto parsed = json::parse(json_text);

  Circuit circuit{};
  circuit.board_width = parsed.at("board").at("width").get<int>();
  circuit.board_height = parsed.at("board").at("height").get<int>();

  for (const auto& component_json : parsed.at("components")) {
    Component component{};
    component.id = component_json.at("id").get<std::string>();
    component.kind = component_json.at("kind").get<std::string>();
    component.width = component_json.at("width").get<int>();
    component.height = component_json.at("height").get<int>();

    for (const auto& pin_json : component_json.at("pins")) {
      Pin pin{};
      pin.id = pin_json.at("id").get<std::string>();
      pin.x = pin_json.at("x").get<int>();
      pin.y = pin_json.at("y").get<int>();
      component.pins.push_back(pin);
    }

    circuit.components.push_back(component);
  }

  for (const auto& net_json : parsed.at("nets")) {
    Net net{};
    net.id = net_json.at("id").get<std::string>();

    for (const auto& connection_json : net_json.at("connections")) {
      Connection connection{};
      connection.component = connection_json.at("component").get<std::string>();
      connection.pin = connection_json.at("pin").get<std::string>();
      net.connections.push_back(connection);
    }

    circuit.nets.push_back(net);
  }

  return circuit;
}

}  // namespace tracemind::model
