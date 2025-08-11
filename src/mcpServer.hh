#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#include "json.hpp"
#include "mcpTool.hh"

using json = nlohmann::json;

/**
 * @brief Simple MCP (Model Context Protocol) server implementation
 *
 * This class implements a server that:
 * - Communicates via JSON-RPC 2.0 protocol over stdin/stdout
 * - Manages a collection of tools that can be called by MCP clients
 * - Handles model context interactions
 * - Logs all exchanges to a file for debugging
 */
class SimpleMCPServer {
private:
  // Instance members
  std::map<std::string, std::unique_ptr<McpTool>>
      fRegisteredTools;       ///< Registry of available tools
  std::string fServerName;    ///< Server name for MCP identification
  std::string fServerVersion; ///< Server version for MCP identification

  // Message handling methods
  void sendResponse(const json &id, const json &result) {
    json response = {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};

    std::string responseStr = response.dump();
    std::cout << responseStr << std::endl;
  }

  void sendError(const json &id, int code, const std::string &message) {
    json response = {{"jsonrpc", "2.0"},
                     {"id", id},
                     {"error", {{"code", code}, {"message", message}}}};

    std::string responseStr = response.dump();
    std::cout << responseStr << std::endl;
  }

  // Request processing methods
  void handleToolsListRequest(const json &id) {
    json tools = json::array();

    for (const auto &toolPair : fRegisteredTools) {
      const auto &tool = toolPair.second;
      // Parse the tool description (which should return valid JSON)
      tools.push_back(json::parse(tool->describe()));
    }

    json result = {{"tools", tools}};
    sendResponse(id, result);
  }

  void handleToolCall(const json &id, const std::string &toolName,
                      const json &arguments) {
    if (fRegisteredTools.find(toolName) == fRegisteredTools.end()) {
      sendError(id, -32602, "Method not found: " + toolName);
      return;
    }

    // Call the tool with JSON arguments
    std::string toolResponse =
        fRegisteredTools[toolName]->call(arguments.dump());

    // Parse the tool response as JSON
    json responseJson;
    try {
      responseJson = json::parse(toolResponse);
    } catch (const json::parse_error &e) {
      // If tool returns plain text, wrap it in proper format
      responseJson = toolResponse;
    }

    json result = {
        {"content", json::array({{{"type", "text"}, {"text", responseJson}}})}};

    sendResponse(id, result);
  }

  void handleInitialize(const json &id) {
    json result = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {{"tools", json::object()}}},
        {"serverInfo", {{"name", fServerName}, {"version", fServerVersion}}}};

    sendResponse(id, result);
  }

public:
  /**
   * @brief Constructor with default server information
   */
  SimpleMCPServer(std::string name)
      : fServerName(name), fServerVersion("1.0.0") {}

  /**
   * @brief Set the server name for MCP identification
   * @param name Server name to display in MCP clients
   */
  void setServerName(const std::string &name) { fServerName = name; }

  /**
   * @brief Set the server version for MCP identification
   * @param version Server version to display in MCP clients
   */
  void setServerVersion(const std::string &version) {
    fServerVersion = version;
  }

  /**
   * @brief Registers a new tool with the MCP server
   * @param tool Unique pointer to the tool to register
   */
  void registerTool(std::unique_ptr<McpTool> tool) {
    std::string name = tool->name();
    fRegisteredTools[name] = std::move(tool);
  }

  /**
   * @brief Starts the MCP server and processes incoming requests
   *
   * This method runs an infinite loop reading JSON-RPC requests from stdin
   * and sending responses to stdout. The server handles:
   * - initialize: Server capability negotiation
   * - tools/list: Returns available tools
   * - tools/call: Executes a specific tool with arguments
   */
  void run() {

    std::string line;

    while (std::getline(std::cin, line)) {
      if (line.empty()) {
        continue;
      }

      try {
        json request = json::parse(line);

        // Extract fields from JSON
        json id = request.value("id", json());
        std::string method = request.value("method", "");

        if (method == "initialize") {
          handleInitialize(id);
        } else if (method == "notifications/cancelled") {
          // nothing to do
        } else if (method == "notifications/initialized") {
          // nothing to do
        } else if (method == "tools/list") {
          handleToolsListRequest(id);
        } else if (method == "tools/call") {
          // Extract tool name and arguments
          json params = request.value("params", json::object());
          std::string toolName = params.value("name", "");
          json arguments = params.value("arguments", json::object());

          handleToolCall(id, toolName, arguments);
        } else {
          sendError(id, -32601, "Method not found: " + method);
        }
      } catch (const json::parse_error &e) {
        sendError(json(), -32700, "Parse error: " + std::string(e.what()));
      }
    }
  }
};