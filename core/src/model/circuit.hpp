#pragma once

#include <string>
#include <vector>

namespace tracemind::model {

struct Pin {
  std::string id;
  int x;
  int y;
};

struct Component {
  std::string id;
  std::string kind;
  int width;
  int height;
  std::vector<Pin> pins;
};

struct Connection {
  std::string component;
  std::string pin;
};

struct Net {
  std::string id;
  std::vector<Connection> connections;
};

struct Circuit {
  int board_width;
  int board_height;
  std::vector<Component> components;
  std::vector<Net> nets;
};

Circuit ParseCircuit(const std::string& json_text);

}  // namespace tracemind::model
