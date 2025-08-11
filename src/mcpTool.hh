#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#include "json.hpp"

using json = nlohmann::json;

// ============================================================================
// MCP Tool Interface
// ============================================================================

/**
 * @brief Abstract base class for MCP tools
 *
 * This interface defines the contract that all MCP tools must implement.
 * Tools represent executable functions that can be invoked by MCP clients.
 */
class McpTool {
public:
  McpTool() = default;
  virtual ~McpTool() = default;

  /**
   * @brief Get the unique name/identifier of this tool
   * @return Tool name used for MCP communication
   */
  virtual std::string name() const = 0;

  /**
   * @brief Get the JSON schema description of this tool
   * @return JSON string describing the tool's interface
   */
  virtual std::string describe() const = 0;

  /**
   * @brief Execute the tool with given arguments
   * @param arguments JSON string containing the tool's input parameters
   * @return JSON array containing MCP-structured content items
   */
  virtual json call(const std::string &arguments) = 0;
};
