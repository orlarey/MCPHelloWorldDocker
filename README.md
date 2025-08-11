# MCPHelloWorldDocker

A minimal Model Context Protocol (MCP) server implementation in C++ running inside a Docker container.

## Introduction

This project implements a basic MCP server from scratch without using MCP libraries or SDKs. The implementation demonstrates the core MCP protocol functionality using only standard C++ and the `json.hpp` library for JSON parsing.

The server provides a single "HelloTool" that accepts a user name and returns a greeting message. The entire application runs in a Docker container for easy deployment and testing.

## MCP Protocol Basics

The Model Context Protocol (MCP) enables communication between Large Language Models (LLMs) like Claude and external servers. The LLM acts as the client and initiates all communications with the server.

In this implementation, communication occurs through standard input/output (stdio) - the LLM sends JSON-RPC 2.0 messages to the server's stdin and receives responses from the server's stdout.

**⚠️ Important:** The server must only output valid JSON messages to stdout, and each JSON message must be on a single line. Any other output (debug messages, logs, etc.) will break the communication with the LLM client.

Here are the key exchanges that occur when an LLM client connects to this server:

### 1. Initialization

The client initiates the connection by sending protocol version information and its capabilities. The server responds with its own protocol version, capabilities, and server information.

**Client → Server:**
```json
{"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"claude-ai","version":"0.1.0"}},"jsonrpc":"2.0","id":0}
```

**Server → Client:**
```json
{"jsonrpc":"2.0","id":0,"result":{"capabilities":{"tools":{}},"protocolVersion":"2024-11-05","serverInfo":{"name":"GreetingServer","version":"1.0.0"}}}
```

### 2. Initialization Complete

After receiving the server's initialization response, the client sends a notification to confirm that initialization is complete and the session is ready. This is a notification message, so the server does not respond.

**Client → Server:**
```json
{"method":"notifications/initialized","jsonrpc":"2.0"}
```

### 3. Tool Discovery

This phase allows the MCP server to expose its capabilities to the client. The client requests a list of available tools, and the server responds with detailed information about each tool including name, description, and input schema.

**Client → Server:**
```json
{"method":"tools/list","params":{},"jsonrpc":"2.0","id":1}
```

**Server → Client:**
```json
{"jsonrpc":"2.0","id":1,"result":{"tools":[{"description":"A tool that greets users","inputSchema":{"properties":{"value":{"description":"User name to greet","type":"string"}},"required":["value"],"type":"object"},"name":"HelloTool"}]}}
```

In this example, the server exposes only one tool named "HelloTool". The response includes:
- `name`: The tool identifier used for calling it
- `description`: Human-readable description of what the tool does
- `inputSchema`: JSON schema defining the expected input parameters (here, a required string parameter called "value")

### 4. Tool Execution

When the client wants to execute a tool, it sends a `tools/call` request with the tool name and arguments. The server processes the request and returns the result in a structured format.

**Client → Server:**
```json
{"method":"tools/call","params":{"name":"HelloTool","arguments":{"value":"Yann"}},"jsonrpc":"2.0","id":4}
```

**Server → Client:**
```json
{"jsonrpc":"2.0","id":4,"result":{"content":[{"text":"Hello-bonjour Yann!","type":"text"}]}}
```

The server's response contains a `content` array with the tool's output. Each content item has a `type` (here "text") and the corresponding data. According to the official MCP specification, the supported content types include:

- **Text content**: `{"type": "text", "text": "Hello World"}`
- **Image content**: `{"type": "image", "data": "base64-encoded-data", "mimeType": "image/png"}`
- **Audio content**: `{"type": "audio", "data": "base64-encoded-audio-data", "mimeType": "audio/wav"}`
- **Resource links**: `{"type": "resource_link", "uri": "file:///path/file.txt", "name": "file.txt"}`
- **Embedded resources**: `{"type": "resource", "resource": {"uri": "file:///path/file.txt", "text": "content"}}`

All content types support optional annotations for metadata like audience and priority.

*For complete content type specifications, see: https://modelcontextprotocol.io/specification/2025-06-18/server/tools*

### JSON-RPC 2.0 Foundation

The Model Context Protocol is built on top of JSON-RPC 2.0, which provides the message structure and communication semantics. Understanding JSON-RPC helps interpret the message exchanges shown above:

**Request Messages** contain:
- `jsonrpc`: Always "2.0" to indicate the protocol version
- `method`: The name of the method to invoke (e.g., "initialize", "tools/list", "tools/call")
- `params`: Parameters for the method call (optional)
- `id`: Unique identifier for request/response correlation

**Response Messages** contain:
- `jsonrpc`: Always "2.0"
- `id`: Matching the request ID for correlation
- `result`: The successful result data, OR
- `error`: Error information with code and message

**Notification Messages** (like "notifications/initialized") contain:
- `jsonrpc`: Always "2.0"
- `method`: The notification method name
- No `id` field (notifications expect no response)

**ID Correlation**: The `id` field is crucial for matching responses to requests in asynchronous communication. When a client sends a request with `"id": 0`, the server must respond with the same `"id": 0`. This allows the client to handle multiple concurrent requests and match each response to its corresponding request. IDs can be numbers, strings, or null, but must be unique for pending requests.

**MCP over JSON-RPC Encapsulation**: The MCP protocol is fully encapsulated within JSON-RPC 2.0 following these principles:

- **Method Namespace**: MCP defines specific method names (e.g., "initialize", "tools/list", "tools/call") that map to MCP operations
- **Parameter Structure**: The `params` field contains MCP-specific data structures for each method
- **Result Format**: Response `result` fields follow MCP-defined schemas for each operation type
- **Error Codes**: MCP uses standard JSON-RPC error codes (-32601 for "Method not found", -32602 for "Invalid params", etc.)
- **Transport Agnostic**: JSON-RPC provides the message format, while MCP defines the semantic meaning and data structures

**Complete Indirection Pattern**: MCP uses a comprehensive indirection mechanism across all aspects:

**1. Method Indirection:**
- **JSON-RPC Level**: The method is always `"tools/call"` - a single, fixed JSON-RPC method
- **MCP Level**: The actual tool to execute (`"HelloTool"`) is passed as a parameter in `params.name`

**2. Parameter Indirection:**
- **JSON-RPC Level**: The `params` contains MCP structure: `{"name": "HelloTool", "arguments": {...}}`
- **Tool Level**: The actual tool parameters (`{"value": "Yann"}`) are nested within `params.arguments`
- **Flow**: JSON-RPC params → MCP arguments → Tool-specific parameters

**3. Result Indirection:**
- **Tool Level**: Tool returns simple data (e.g., `"Hello Yann!"`)
- **MCP Level**: Result is wrapped in MCP content structure: `{"content": [{"type": "text", "text": "Hello Yann!"}]}`
- **JSON-RPC Level**: Final response follows JSON-RPC format: `{"jsonrpc": "2.0", "id": 4, "result": {...}}`

This triple-layer indirection provides complete abstraction - tools operate with simple inputs/outputs while benefiting from standardized transport and content formatting.

**Visual Example of Protocol Layering:**

```json
// Tool Call Request - Protocol Layer Breakdown
{
  // 🔵 JSON-RPC 2.0 Layer (Transport & Correlation)
  "jsonrpc": "2.0",           // Protocol version
  "id": 4,                    // Request correlation ID
  "method": "tools/call",     // Fixed JSON-RPC method name
  
  // 🟠 MCP Layer (Semantic & Routing)
  "params": {
    "name": "HelloTool",      // MCP tool identifier
    "arguments": {
      // 🟢 Tool Layer (Application Logic)
      "value": "Yann"         // Tool-specific parameter
    }
  }
}

// Tool Call Response - Protocol Layer Breakdown
{
  // 🔵 JSON-RPC 2.0 Layer (Transport & Correlation)
  "jsonrpc": "2.0",           // Protocol version
  "id": 4,                    // Matching request ID
  
  // 🟠 MCP Layer (Content Standardization)
  "result": {
    "content": [{             // MCP content wrapper
      "type": "text",         // MCP content type
      "text": "Hello Yann!"   // 🟢 Tool output (wrapped)
    }]
  }
}
```

```json
// Tool Discovery Response - Protocol Layer Breakdown
{
  // 🔵 JSON-RPC 2.0 Layer (Transport & Correlation)
  "jsonrpc": "2.0",
  "id": 1,
  
  // 🟠 MCP Layer (Tool Registry & Schema Format)
  "result": {
    "tools": [{
      // 🟢 Tool Layer (Tool Identity)
      "name": "HelloTool",
      "description": "A tool that greets users",
      
      // 🟠 MCP Layer (Parameter Description Standard)
      "inputSchema": {           // MCP-defined way to describe parameters
        "type": "object",        // JSON Schema format (MCP standard)
        "properties": {
          // 🟢 Tool Layer (Tool-specific Parameters)
          "value": {             // Tool defines what it needs
            "type": "string",
            "description": "User name to greet"
          }
        },
        "required": ["value"]
      }
      
      // Note: outputSchema is optional in MCP spec
      // When present, it describes the structure of tool results
    }]
  }
}
```

**Key Insight**: The `inputSchema` uses **MCP-standardized JSON Schema format** to describe **tool-specific parameters**. MCP defines HOW to describe parameters (JSON Schema), while tools define WHAT parameters they need.

**Legend:**
- 🔵 **JSON-RPC Layer**: Message structure, correlation, transport
- 🟠 **MCP Layer**: Standardized formats, schemas, content types
- 🟢 **Tool Layer**: Tool-specific logic, parameters, identity

This layered approach separates concerns: JSON-RPC handles message structure and correlation, while MCP defines the application-level protocol semantics.

### Protocol Grammar (BNF Notation)

The following BNF grammars show how each protocol layer constrains and extends the previous one:

**1. JSON-RPC 2.0 Layer (Transport Foundation)**
```bnf
<jsonrpc-message> ::= <jsonrpc-request> | <jsonrpc-response> | <jsonrpc-notification>

<jsonrpc-request> ::= { jsonrpc: 2.0, method: <method-name>, params?: <params>, id: <id> }
<jsonrpc-notification> ::= { jsonrpc: 2.0, method: <method-name>, params?: <params> }
<jsonrpc-response> ::= { jsonrpc: 2.0, id: <id>, result: <result> }
                     | { jsonrpc: 2.0, id: <id>, error: <error-object> }

<error-object> ::= { code: <number>, message: <string> }
<method-name> ::= <string>  // Generic - any string allowed
<params> ::= <any-json>     // Generic - any JSON structure  
<result> ::= <any-json>     // Generic - any JSON structure
<id> ::= <number> | <string> | null
```

**2. MCP Layer (Constrains JSON-RPC)**
```bnf
// MCP constrains JSON-RPC method names to specific values
<method-name> ::= <mcp-method>
<mcp-method> ::= initialize | tools/list | tools/call | notifications/initialized

// MCP constrains params structure based on method
<params> ::= <mcp-params>
<mcp-params> ::= <init-params> | <tools-list-params> | <tools-call-params> | <empty>

<init-params> ::= { protocolVersion: <string>, capabilities: <object>, clientInfo: <object> }
<tools-call-params> ::= { name: <tool-name>, arguments: <tool-arguments> }
<tools-list-params> ::= {}
<empty> ::= // notifications/initialized has no params

// MCP constrains result structure based on method
<result> ::= <mcp-result>
<mcp-result> ::= <init-result> | <tools-list-result> | <tools-call-result>

<init-result> ::= { protocolVersion: <string>, capabilities: <object>, serverInfo: <object> }
<tools-call-result> ::= { content: [<content-item>*], isError?: <boolean> }
<tools-list-result> ::= { tools: [<tool-schema>*] }

<content-item> ::= { type: <content-type> } & <content-data>
<content-type> ::= text | image | audio | resource_link | resource
<content-data> ::= { text: <string> }  // Simplified - each type has specific fields
<tool-schema> ::= { name: <tool-name>, description: <string>, inputSchema: <json-schema> }
```

**3. HelloWorld Server Layer (Constrains MCP)**
```bnf
// Server constrains MCP tool names to specific implementations
<tool-name> ::= <hello-tool-name>
<hello-tool-name> ::= HelloTool

// Server constrains MCP tool arguments to specific schemas
<tool-arguments> ::= <hello-tool-arguments>
<hello-tool-arguments> ::= { value: <string> }

// Server constrains MCP tool schemas to specific definitions  
<tool-schema> ::= <hello-tool-schema>
<hello-tool-schema> ::= { name: HelloTool,
                          description: "A tool that greets users",
                          inputSchema: <hello-input-schema> }

<hello-input-schema> ::= { type: object,
                           properties: { value: { type: string, description: User name to greet } },
                           required: [value, ...] }

// Example: Multi-parameter tool schema
<multi-param-schema> ::= { type: object,
                           properties: { 
                             name: { type: string, description: User name },
                             age: { type: number, description: User age },
                             greeting: { type: string, description: Greeting style, enum: [formal, casual, ...] }
                           },
                           required: [name, age, ...] }
```

**Key Insight**: Each layer **constrains** the more generic layer below it:
- JSON-RPC allows any `method` → MCP restricts to specific methods
- JSON-RPC allows any `params` → MCP defines structure per method  
- MCP allows any `tool-name` → Server implements specific tools

**Architecture References:**
- MCP Architecture Overview: https://modelcontextprotocol.io/docs/learn/architecture
- Protocol Mechanics: https://pradeepl.com/blog/model-context-protocol/mcp-protocol-mechanics-and-architecture/
- JSON-RPC 2.0 specification: https://www.jsonrpc.org/specification

## Project Structure

### Dependencies

The project uses only one external library:

**json.hpp** - A single-header JSON library for C++
- Header-only library requiring no compilation or installation
- Provides intuitive JSON parsing and generation with modern C++ syntax
- Chosen for its simplicity and zero-dependency approach, perfect for this minimal MCP implementation
- Allows easy conversion between C++ objects and JSON strings required for MCP protocol communication

### Core Classes

**mcpTool.hh** - Abstract base class for MCP tools

The `McpTool` class defines the interface that all MCP tools must implement. It contains three pure virtual methods:

- `name()` - Returns the unique identifier for the tool used in MCP communication
- `describe()` - Returns a JSON string describing the tool's schema, including its description and input parameters
- `call(const std::string &arguments)` - Executes the tool with the provided JSON arguments and returns a JSON response

New functionality is added to the MCP server by creating classes that inherit from `McpTool` and implement these three methods. The inheritance pattern allows the server to manage different tools uniformly while each tool implements its specific logic.

**mcpServer.hh** - MCP server implementation

The `SimpleMCPServer` class implements the core MCP protocol functionality:

- **Tool Management**: Maintains a registry of available tools using `std::map<std::string, std::unique_ptr<McpTool>>`
- **Protocol Handling**: Processes JSON-RPC 2.0 messages received from stdin
- **Message Processing**: Handles three main types of requests:
  - `initialize` - Server capability negotiation with protocol version and server information
  - `tools/list` - Returns the list of available tools by calling each tool's `describe()` method
  - `tools/call` - Executes a specific tool with provided arguments and returns the result
- **Response Generation**: Formats responses according to MCP protocol specifications and sends them to stdout
- **Error Handling**: Sends appropriate JSON-RPC error responses for invalid requests or missing tools

The server operates by reading JSON messages line-by-line from stdin, parsing them, and dispatching to the appropriate handler methods. All responses are sent to stdout as single-line JSON messages.

**hello.cpp** - Main application and HelloTool implementation

This file contains the main application that demonstrates how to use the MCP framework:

- **HelloTool class**: Inherits from `McpTool` and implements a simple greeting tool
  - `name()` returns "HelloTool" as the tool identifier
  - `describe()` returns a JSON schema describing the tool's purpose and input parameters (requires a "value" string parameter for the user name)
  - `call()` parses the JSON arguments, extracts the "value" parameter, creates a greeting message, and returns it as JSON
- **Main function**: Creates a `SimpleMCPServer` instance named "GreetingServer", registers the HelloTool, and starts the server loop
- **Error handling**: Includes JSON parsing error handling that returns appropriate error messages

The HelloTool serves as a template showing how to implement custom MCP tools by inheriting from the base class and providing the required functionality.

## Building and Running

### Docker Setup

The project includes a Dockerfile that creates a containerized environment for the MCP server:

```dockerfile
FROM gcc:latest
WORKDIR /app
COPY src/ ./src/
RUN g++ -std=c++17 src/hello.cpp -o hello
ENTRYPOINT ["/app/hello"]
```

The Dockerfile:
- Uses `gcc:latest` as the base image, providing a complete C++ compilation environment
- Sets `/app` as the working directory
- Copies the source files into the container
- Compiles the application using g++ with C++17 standard
- Sets the compiled binary as the container's entry point

### Building the Image

Run the provided build script:

```bash
./build.sh
```

Or build manually:

```bash
docker build -t mcp-hello-app .
```

### MCP Client Configuration

To use this server with an MCP client, add the following configuration:

```json
{
  "mcpServers": {
    "dockermcphello": {
      "command": "docker",
      "args": ["run", "-i", "--rm", "mcp-hello-app"]
    }
  }
}
```

The `-i` flag enables interactive mode, allowing the client to send input to the container's stdin and receive output from stdout. The `--rm` flag automatically removes the container when it exits, preventing accumulation of stopped containers.

## References

For more detailed information about the Model Context Protocol:

- **Official Specification**: https://modelcontextprotocol.io/specification/2025-06-18
- **Official Documentation**: https://modelcontextprotocol.io/
- **GitHub Repository**: https://github.com/modelcontextprotocol/modelcontextprotocol
