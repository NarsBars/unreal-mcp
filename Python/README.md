# Unreal MCP — Python Bridge

Python side of the **Unreal MCP** plugin: a [Model Context Protocol](https://modelcontextprotocol.io/) server that exposes ~120 Unreal Engine editor tools to MCP clients (Claude Code, Claude Desktop, Cursor, Windsurf, etc.).

Pairs with the C++ plugin under `Plugins/UnrealMCP/Source/`, which listens on a TCP socket inside the running Unreal Editor process. This Python server forwards MCP tool calls to that socket and returns the editor's JSON responses.

## Setup

1. **Python 3.10+**
2. **Install `uv`** if you don't have it:
   ```bash
   curl -LsSf https://astral.sh/uv/install.sh | sh
   ```
3. **Create a virtualenv and install dependencies:**
   ```bash
   uv venv
   # On Unix/macOS:
   source .venv/bin/activate
   # On Windows:
   .venv\Scripts\activate

   uv pip install -e .
   ```
4. **Add the server to your MCP client.** Example for Claude Code (`.mcp.json`):
   ```json
   {
     "mcpServers": {
       "unrealMCP": {
         "command": "uv",
         "args": ["run", "--directory", "<path-to>/Plugins/UnrealMCP/Python", "python", "unreal_mcp_server.py"]
       }
     }
   }
   ```

Start your Unreal Editor with the plugin enabled. The C++ side binds to `127.0.0.1:55557` by default. The Python server connects on tool invocation; you don't need to start it manually.

## Multi-Project / Multi-Editor Setup

The bridge supports running two or more editors simultaneously, each on a different port. The editor parses `-MCPPort=<port>` at launch (see `UnrealMCPBridge::Initialize` in C++). The Python side reads three env vars:

| Env var | Default | Purpose |
|---------|---------|---------|
| `UNREAL_MCP_HOST` | `127.0.0.1` | Host the bridge listens on |
| `UNREAL_MCP_PORT` | `55557` | Port the bridge listens on (must match the editor's `-MCPPort=`) |
| `UNREAL_MCP_NAME` | `UnrealMCP` | FastMCP server name + log filename suffix; must be unique per instance |

Example: register two MCP server entries — one for your primary project on port `55557`, one for a reference project on `55558` — by passing different `UNREAL_MCP_PORT` and `UNREAL_MCP_NAME` env vars to each.

Launch a secondary editor pointing at a different project:

```powershell
& "<path-to>/UnrealEditor.exe" `
  "<path-to>/OtherProject.uproject" `
  -MCPPort=55558
```

Verify both are reachable with `get_project_info` against each MCP server.

The `launch_editor` tool also accepts `UNREAL_MCP_PROJECT_PATH` and `UNREAL_MCP_EDITOR_PATH` env vars as defaults for its arguments.

## PIE Input Driving

Three tools automate input-driven testing in Play-In-Editor sessions. They wrap a C++ async ticker; the Python side hides the start-and-poll dance and returns a time series of pawn samples.

| Tool | Purpose |
|---|---|
| `pie_drive_input` | Drive `AddMovementInput` for a duration; sample location/velocity/control-rotation. Pass either `direction_world=[x,y,z]` or `direction_named="Forward"/"Right"/...`. Optional `pin_yaw`/`pin_pitch` lock the camera each tick. |
| `pie_simulate_key` | Hold an Enhanced Input action 'pressed' for `pressed_for_seconds`. Resolves names against `/Game/Input/Actions/IA_<Name>` by convention; full asset paths starting with `/Game/` also accepted. |
| `pie_set_control_rotation` | One-shot setter for the player controller's control rotation. To pin while driving, pass `pin_yaw`/`pin_pitch` to `pie_drive_input` instead. |

### Sample shape

```python
{
  "done": True,
  "elapsed": 2.05,
  "duration": 2.0,
  "samples": [
    {
      "time": 0.10,
      "location": {"x": 100.0, "y": 0.0, "z": 92.0},
      "velocity": {"x": 250.0, "y": 0.0, "z": 0.0},
      "control_rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
      "gmc_movement_mode": "Grounded",   # only when the GMC plugin is present
      "active_tags": ["State.Combat.Idle", "..."]  # only when GMC is present
    },
    ...
  ]
}
```

> **GMC telemetry** (`gmc_movement_mode`, `active_tags`) is **optional** and only compiled in when the [Generic Movement Component](https://www.unrealengine.com/marketplace/en-US/product/generic-movement-component-gmc) plugin is detected in the project's `Plugins/` folder at build time. Projects without GMC get a working PIE driver with these fields absent or empty.

### Strafe-test example

```python
# 1. start PIE
start_pie()

# 2. point the camera at world +X
pie_set_control_rotation(yaw=0)

# 3. strafe right for one second; camera locked at yaw=0 the whole time
result = pie_drive_input(
    duration_sec=1.0,
    direction_named="Right",
    pin_yaw=0,
    sample_dt_sec=0.1,
)

# 4. assert lateral motion
final = result["samples"][-1]
assert final["location"]["y"] > 50, f"Expected +Y strafe, got {final['location']}"
```

### Architecture note

Each "start" command registers an `FTSTicker` and returns a `job_id` immediately. The ticker accumulates samples each frame and unregisters itself when the duration elapses. The Python wrapper polls `pie_get_job_result` until `done: true` and returns the final time series. A 5-second safety margin is added to the deadline; on timeout the wrapper sends `pie_cancel_job`. This is required because `MCPServerRunnable` blocks the worker on the game-thread future — a long synchronous body would freeze the editor.

If the pawn or PlayerController is invalidated mid-job (e.g. PIE stopped), the next `pie_get_job_result` returns `done: true` with `error: "Pawn or PC invalidated mid-job"` and the partial sample buffer up to that point.

## Testing Scripts

The [`scripts/`](./scripts) folder has standalone Python scripts that talk to the C++ bridge directly without going through MCP. Useful for smoke-testing tools and debugging the bridge without a running MCP server. Run inside the `uv` virtualenv.

## Troubleshooting

- Make sure the Unreal Editor is running before invoking tools — the Python server only connects on demand and will fail fast if nothing is listening on the configured port.
- Logs default to `unreal_mcp.log` next to the Python entrypoint. With `UNREAL_MCP_NAME` set, each instance gets a unique log file (e.g. `unreal_mcp_UnrealMCP_GASP.log`).
- C++-side errors appear in the Unreal Editor's Output Log under the `LogUnrealMCP` category.

## Development

To add new tools:

1. Add a new command handler on the C++ side (see `Source/UnrealMCP/Private/Commands/`).
2. Route the new command in `UnrealMCPBridge::HandleCommand`.
3. Expose it through a Python wrapper in `tools/<category>_tools.py`, decorating with `@mcp.tool()`.
4. The MCP client picks it up on next server restart.

The category modules in `tools/` are intentionally one-file-per-domain (animation, blueprint, material, etc.) so the surface is grep-friendly.
