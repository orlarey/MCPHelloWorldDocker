# Base image with C++ compilation tools
FROM gcc:latest

# Set working directory
WORKDIR /app

# Copy source files
COPY src/ ./src/

# Compiles the application
RUN g++ -std=c++17 src/hello.cpp -o hello

# Set entry point
ENTRYPOINT ["/app/hello"]


# "mcpServers": {
#     "dockermcphello": {
#         "command": "docker",
#         "args": ["run", "-i", "mcp-hello-app"]
#     }
# }
