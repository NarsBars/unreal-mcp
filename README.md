<div align="center">

# Model Context Protocol for Unreal Engine
<span style="color: #555555">unreal-mcp</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/chongdashu/unreal-mcp)

</div>

This project enables AI assistant clients like Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP).

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state. The API, functionality, and implementation details are subject to significant changes. While we encourage testing and feedback, please be aware that:

- Breaking changes may occur without notice
- Features may be incomplete or unstable
- Documentation may be outdated or missing
- Production use is not recommended at this time

## NarsBars / unreal-mcp — Fork Additions

This is an actively-maintained fork of [@chongdashu](https://github.com/chongdashu)'s [unreal-mcp](https://github.com/chongdashu/unreal-mcp) (upstream last updated ~April 2025) with substantially expanded tool coverage built for **agent-driven UE5 editor automation** on a production game project.

**Surface area:** **~120 MCP tools across 13 Python categories**, backed by 17 C++ command modules. Counted from `@mcp.tool()` decorators in [`Python/tools/`](Python/tools/):

| Category | Tools | What it covers |
|---|---:|---|
| **animation_tools** | 35 | Anim BP creation, anim graph node authoring, state machines, transitions, blend nodes, motion-matching configuration, history collector setup, chooser tables, anim notifies, mirroring properties |
| **material_tools** | 19 | Material expression authoring + connections, material instance parameters (scalar/vector/texture/static-switch), custom HLSL expressions, parameter collections, recompile flow |
| **editor_tools** | 13 | Editor lifecycle, project info, log retrieval, console commands, embedded Python, screenshots, PIE state + PIE input driving (drive movement, simulate keys, set control rotation) |
| **data_asset_tools** | 11 | Generic UDataAsset creation, property get/set, structured imports for any UPROPERTY shape |
| **node_tools** | 9 | K2 node spawning, smart pin connection, blueprint event/function node authoring |
| **asset_tools** | 8 | Bulk creation, dependency inspection, search, rename/move, duplicate, delete |
| **umg_tools** | 6 | UMG widget editing surface |
| **blueprint_tools** | 5 | Blueprint creation, function/variable management, component attachment |
| **audio_tools** | 4 | Sound class hierarchy, sound mix authoring |
| **input_tools** | 4 | Enhanced Input action/mapping context creation |
| **chooser_tools** | 2 | Chooser table reading + column value editing |
| **niagara_tools** | 2 | Niagara emitter + system creation |
| **project_tools** | 1 | Project-wide info retrieval |

### Notable additions over upstream

- **PIE Input Driving** (`pie_drive_input`, `pie_simulate_key`, `pie_set_control_rotation`) — C++ async ticker wrapped in a polling Python facade. Drives an actual gameplay session in PIE, samples pawn state (location, velocity, control rotation, optional GMC movement mode + active tags) at fixed intervals, and pins yaw/pitch while moving. Lets agents run regression checks on movement, abilities, and animation.
- **Multi-project / multi-editor support** — `-MCPPort=<port>` editor-side launch arg + `UNREAL_MCP_HOST` / `UNREAL_MCP_PORT` / `UNREAL_MCP_NAME` env vars enable two or more UE editors to run side-by-side on distinct MCP ports, each surfaced as its own FastMCP server instance.
- **Animation-pipeline depth** — graph node authoring, state machine editing, motion-matching + history collector + chooser table configuration, anim notify and notify-state management. Enough surface for agent-driven anim graph construction beyond simple property setting.
- **Material-pipeline depth** — expression-level authoring (add, connect, disconnect, properties, custom HLSL), all four material-instance parameter types, parameter collection creation, full recompile control.
- **Smart pin connection** (`smart_connect_pins`) — type-aware blueprint pin resolution that picks the right pin pair from a partial spec, reducing common LLM mistakes when wiring K2 graphs.
- **Optional GMC support** — auto-detected at build time. The PIE driver picks up [Generic Movement Component](https://www.unrealengine.com/marketplace/en-US/product/generic-movement-component-gmc) movement mode + active gameplay tags when GMC is present in the project's `Plugins/` folder. Compiles cleanly without GMC; non-GMC users just don't get those telemetry fields.

### What stays from upstream

The foundation — TCP bridge transport, command dispatch architecture, base blueprint and asset commands — comes from the upstream project. All credit for the original design and initial implementation goes to [@chongdashu](https://github.com/chongdashu).

### License

Upstream is MIT-licensed. This fork remains MIT-licensed. Additions made in this fork carry the same MIT terms; see [LICENSE](LICENSE) for the full notice and attribution to [@chongdashu](https://github.com/chongdashu) for the original work.

### Status

Tracking UE 5.7. Tested against an active production game project. The fork is maintained independently because the projects' use cases have diverged; PRs back to upstream are welcome.


## 🌟 Overview

The Unreal MCP integration provides comprehensive tools for controlling Unreal Engine through natural language:

| Category | Capabilities |
|----------|-------------|
| **Actor Management** | • Create and delete actors (cubes, spheres, lights, cameras, etc.)<br>• Set actor transforms (position, rotation, scale)<br>• Query actor properties and find actors by name<br>• List all actors in the current level |
| **Blueprint Development** | • Create new Blueprint classes with custom components<br>• Add and configure components (mesh, camera, light, etc.)<br>• Set component properties and physics settings<br>• Compile Blueprints and spawn Blueprint actors<br>• Create input mappings for player controls |
| **Blueprint Node Graph** | • Add event nodes (BeginPlay, Tick, etc.)<br>• Create function call nodes and connect them<br>• Add variables with custom types and default values<br>• Create component and self references<br>• Find and manage nodes in the graph |
| **Editor Control** | • Focus viewport on specific actors or locations<br>• Control viewport camera orientation and distance |

All these capabilities are accessible through natural language commands via AI assistants, making it easy to automate and control Unreal Engine workflows.

## 🧩 Components

### Sample Project (MCPGameProject) `MCPGameProject`
- Based off the Blank Project, but with the UnrealMCP plugin added.

### Plugin (UnrealMCP) `MCPGameProject/Plugins/UnrealMCP`
- Native TCP server for MCP communication
- Integrates with Unreal Editor subsystems
- Implements actor manipulation tools
- Handles command execution and response handling

### Python MCP Server `Python/unreal_mcp_server.py`
- Implemented in `unreal_mcp_server.py`
- Manages TCP socket connections to the C++ plugin (port 55557)
- Handles command serialization and response parsing
- Provides error handling and connection management
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## 📂 Directory Structure

- **MCPGameProject/** - Example Unreal project
  - **Plugins/UnrealMCP/** - C++ plugin source
    - **Source/UnrealMCP/** - Plugin source code
    - **UnrealMCP.uplugin** - Plugin definition

- **Python/** - Python server and tools
  - **tools/** - Tool modules for actor, editor, and blueprint operations
  - **scripts/** - Example scripts and demos

- **Docs/** - Comprehensive documentation
  - See [Docs/README.md](Docs/README.md) for documentation index

## 🚀 Quick Start Guide

### Prerequisites
- Unreal Engine 5.5+
- Python 3.12+
- MCP Client (e.g., Claude Desktop, Cursor, Windsurf)

### Sample project

For getting started quickly, feel free to use the starter project in `MCPGameProject`. This is a UE 5.5 Blank Starter Project with the `UnrealMCP.uplugin` already configured. 

1. **Prepare the project**
   - Right-click your .uproject file
   - Generate Visual Studio project files
2. **Build the project (including the plugin)**
   - Open solution (`.sln`)
   - Choose `Development Editor` as your target.
   - Build

### Plugin
Otherwise, if you want to use the plugin in your existing project:

1. **Copy the plugin to your project**
   - Copy `MCPGameProject/Plugins/UnrealMCP` to your project's Plugins folder

2. **Enable the plugin**
   - Edit > Plugins
   - Find "UnrealMCP" in Editor category
   - Enable the plugin
   - Restart editor when prompted

3. **Build the plugin**
   - Right-click your .uproject file
   - Generate Visual Studio project files
   - Open solution (`.sln)
   - Build with your target platform and output settings

### Python Server Setup

See [Python/README.md](Python/README.md) for detailed Python setup instructions, including:
- Setting up your Python environment
- Running the MCP server
- Using direct or server-based connections

### Configuring your MCP Client

Use the following JSON for your mcp configuration based on your MCP client.

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<path/to/the/folder/PYTHON>",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

An example is found in `mcp.json`

### MCP Configuration Locations

Depending on which MCP client you're using, the configuration file location will differ:

| MCP Client | Configuration File Location | Notes |
|------------|------------------------------|-------|
| Claude Desktop | `~/.config/claude-desktop/mcp.json` | On Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` | Located in your project root directory |
| Windsurf | `~/.config/windsurf/mcp.json` | On Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

Each client uses the same JSON format as shown in the example above. 
Simply place the configuration in the appropriate location for your MCP client.


## License
MIT

## Questions

For questions, you can reach me on X/Twitter: [@chongdashu](https://www.x.com/chongdashu)