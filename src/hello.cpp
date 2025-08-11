#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"
#include "mcpServer.hh"
#include "mcpTool.hh"

using json = nlohmann::json;

// Base64 encoding function
std::string base64_encode(const std::vector<unsigned char> &data) {
  const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  int val = 0, valb = -6;
  for (unsigned char c : data) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      result.push_back(chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
  while (result.size() % 4)
    result.push_back('=');
  return result;
}

// Encode file to base64 and return JSON
std::optional<json> encodeFile(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    return std::nullopt;
  }

  // Read file content
  std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
  file.close();

  if (buffer.empty()) {
    return std::nullopt;
  }

  // Encode to base64
  std::string encoded = base64_encode(buffer);

  // Determine MIME type based on file extension
  std::string mimeType = "application/octet-stream";
  size_t dotPos = filepath.find_last_of('.');
  if (dotPos != std::string::npos) {
    std::string ext = filepath.substr(dotPos + 1);
    // Images
    if (ext == "png")
      mimeType = "image/png";
    else if (ext == "jpg" || ext == "jpeg")
      mimeType = "image/jpeg";
    else if (ext == "gif")
      mimeType = "image/gif";
    else if (ext == "svg")
      mimeType = "image/svg+xml";
    // Source code files
    else if (ext == "cpp" || ext == "cxx" || ext == "cc")
      mimeType = "text/x-c++src";
    else if (ext == "c")
      mimeType = "text/x-csrc";
    else if (ext == "h" || ext == "hh" || ext == "hpp")
      mimeType = "text/x-c++hdr";
    else if (ext == "js")
      mimeType = "text/javascript";
    else if (ext == "ts")
      mimeType = "text/typescript";
    else if (ext == "py")
      mimeType = "text/x-python";
    else if (ext == "java")
      mimeType = "text/x-java";
    else if (ext == "rs")
      mimeType = "text/x-rust";
    else if (ext == "go")
      mimeType = "text/x-go";
    else if (ext == "html")
      mimeType = "text/html";
    else if (ext == "css")
      mimeType = "text/css";
    else if (ext == "xml")
      mimeType = "text/xml";
    else if (ext == "json")
      mimeType = "application/json";
    else if (ext == "md")
      mimeType = "text/markdown";
    // Generic text
    else if (ext == "txt")
      mimeType = "text/plain";
    else if (ext == "pdf")
      mimeType = "application/pdf";
  }

  // Create JSON response - use text for text files, base64 for binary
  json result = {{"mimeType", mimeType}};
  
  if (mimeType.substr(0, 5) == "text/" || mimeType == "application/json") {
    // For text files, return as plain text
    std::string textContent(buffer.begin(), buffer.end());
    result["text"] = textContent;
  } else {
    // For binary files, return as base64
    result["data"] = encoded;
  }

  return result;
}

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
             {{"type", "string"}, {"description", "User name to greet"}}},
            {"birthday",
             {{"type", "string"}, {"description", "User's birthday"}}}}}}},
        {"required", json::array({"value"})}};

    return description.dump();
  }

  json call(const std::string &args) override {
    try {
      // Parse the JSON arguments
      json arguments = json::parse(args);

      // Extract the 'value' field
      std::string userName = arguments.value("value", "World");
      std::string userBirthday = arguments.value("birthday", "");

      // Create the greeting message
      if (!userBirthday.empty()) {
        userName += " (born on " + userBirthday + ")";
      }
      std::string greeting = "Hello " + userName + "!";

      // Return as MCP content array
      return json::array({{{"type", "text"}, {"text", greeting}}});

    } catch (const json::parse_error &e) {
      // Handle parse error
      return json::array(
          {{{"type", "text"}, {"text", "Error: Invalid arguments"}}});
    }
  }
};

class GetSourceCodeTool : public McpTool {
public:
  GetSourceCodeTool() {
    // Initialize tool-specific data here
  }

  std::string name() const override { return "GetSourceCode"; }

  std::string describe() const override {
    // Build tool description using JSON object
    json description = {
        {"name", name()},
        {"description", "Gets the source code of hello.cpp file"},
        {"inputSchema",
         {{"type", "object"},
          {"properties", json::object()},
          {"required", json::array()}}}};

    return description.dump();
  }

  json call(const std::string &args) override {
    try {
      // No arguments needed - just read hello.cpp
      auto fileData = encodeFile("src/hello.cpp");

      if (!fileData) {
        // Return error if file not found
        return json::array(
            {{{"type", "text"},
              {"text", "Error: Could not read hello.cpp file"}}});
      }

      // Return as MCP content array with resource
      // Merge encodeFile result with URI
      json resource = *fileData;
      resource["uri"] = "file://src/hello.cpp";
      
      return json::array({{{"type", "resource"}, {"resource", resource}}});

    } catch (const json::parse_error &e) {
      return json::array(
          {{{"type", "text"}, {"text", "Error: Invalid arguments"}}});
    }
  }
};

int main() {
  SimpleMCPServer server("GreetingServer");
  server.registerTool(std::make_unique<HelloTool>());
  server.registerTool(std::make_unique<GetSourceCodeTool>());
  server.run();
  return 0;
}