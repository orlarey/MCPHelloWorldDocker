#include <iostream>
#include <string>

#include "json.hpp"
#include "mcpServer.hh"
#include "mcpTool.hh"

using json = nlohmann::json;

class HelloTool : public McpTool {
public:
  HelloTool() {
    // Initialize tool-specific data here
  }

  std::string name() const override { return "HelloTool"; }

  std::string describe() const override {
    // Build tool description using JSON object
    json description = {
        {"name", name()},
        {"description", "A tool that greets users"},
        {"inputSchema",
         {{"type", "object"},
          {"properties",
           {{"value",
             {{"type", "string"}, {"description", "User name to greet"}}}}},
          {"required", json::array({"value"})}}}};

    return description.dump();
  }

  std::string call(const std::string &args) override {
    try {
      // Parse the JSON arguments
      json arguments = json::parse(args);

      // Extract the 'value' field
      std::string userName = arguments.value("value", "World");

      // Create the greeting message
      std::string greeting = "Hello " + userName + "!";

      // Return as JSON string
      return json(greeting).dump();

    } catch (const json::parse_error &e) {
      // Handle parse error
      return json("Error: Invalid arguments").dump();
    }
  }
};

int main() {
  SimpleMCPServer server("GreetingServer");
  server.registerTool(std::make_unique<HelloTool>());
  server.run();
  return 0;
}