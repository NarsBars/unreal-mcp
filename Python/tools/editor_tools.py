"""
Editor Tools for Unreal MCP.

This module provides tools for controlling the Unreal Editor viewport and other editor functionality.
"""

import logging
import os
import time
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_editor_tools(mcp: FastMCP):
    """Register editor tools with the MCP server."""
    
    # @mcp.tool()  # Level editing - disabled
    def get_actors_in_level(ctx: Context) -> List[Dict[str, Any]]:
        """Get a list of all actors in the current level."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return []
                
            response = unreal.send_command("get_actors_in_level", {})
            
            if not response:
                logger.warning("No response from Unreal Engine")
                return []
                
            # Log the complete response for debugging
            logger.info(f"Complete response from Unreal: {response}")
            
            # Check response format
            if "result" in response and "actors" in response["result"]:
                actors = response["result"]["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return actors
            elif "actors" in response:
                actors = response["actors"]
                logger.info(f"Found {len(actors)} actors in level")
                return actors
                
            logger.warning(f"Unexpected response format: {response}")
            return []
            
        except Exception as e:
            logger.error(f"Error getting actors: {e}")
            return []

    # @mcp.tool()  # Level editing - disabled
    def find_actors_by_name(ctx: Context, pattern: str) -> List[str]:
        """Find actors by name pattern."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.warning("Failed to connect to Unreal Engine")
                return []
                
            response = unreal.send_command("find_actors_by_name", {
                "pattern": pattern
            })
            
            if not response:
                return []
                
            return response.get("actors", [])
            
        except Exception as e:
            logger.error(f"Error finding actors: {e}")
            return []
    
    # @mcp.tool()  # Level editing - disabled
    def spawn_actor(
        ctx: Context,
        name: str,
        type: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0],
        scale: List[float] = [1.0, 1.0, 1.0]
    ) -> Dict[str, Any]:
        """Create a new actor in the current level.

        Supports any actor class — use the class name (e.g. StaticMeshActor, PointLight,
        ExponentialHeightFog, DecalActor, AudioVolume). Common shortcuts like StaticMeshActor
        and PointLight work directly. For other classes, the engine will search by exact name,
        A-prefix, and /Script/ paths.

        Args:
            ctx: The MCP context
            name: The name to give the new actor (must be unique)
            type: The actor class to spawn (e.g. StaticMeshActor, PointLight, ExponentialHeightFog)
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            scale: The [x, y, z] scale factors (default [1,1,1])

        Returns:
            Dict containing the created actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "name": name,
                "type": type,
                "location": location,
                "rotation": rotation,
                "scale": scale
            }

            # Validate location, rotation, and scale formats
            for param_name in ["location", "rotation", "scale"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"error": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Creating actor '{name}' of type '{type}' with params: {params}")
            response = unreal.send_command("spawn_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}
            
            # Log the complete response for debugging
            logger.info(f"Actor creation response: {response}")
            
            # Handle error responses correctly
            if response.get("status") == "error":
                error_message = response.get("error", "Unknown error")
                logger.error(f"Error creating actor: {error_message}")
                return {"error": error_message}
            
            return response
            
        except Exception as e:
            error_msg = f"Error creating actor: {e}"
            logger.error(error_msg)
            return {"error": error_msg}
    
    # @mcp.tool()  # Level editing - disabled
    def delete_actor(ctx: Context, name: str) -> Dict[str, Any]:
        """Delete an actor by name."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("delete_actor", {
                "name": name
            })
            return response or {"error": "No response from Unreal Engine"}

        except Exception as e:
            logger.error(f"Error deleting actor: {e}")
            return {"error": f"delete_actor failed: {e}"}
    
    # @mcp.tool()  # Level editing - disabled
    def set_actor_transform(
        ctx: Context,
        name: str,
        location: List[float]  = None,
        rotation: List[float]  = None,
        scale: List[float] = None
    ) -> Dict[str, Any]:
        """Set the transform of an actor."""
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}
                
            params = {"name": name}
            if location is not None:
                params["location"] = location
            if rotation is not None:
                params["rotation"] = rotation
            if scale is not None:
                params["scale"] = scale
                
            response = unreal.send_command("set_actor_transform", params)
            return response or {"error": "No response from Unreal Engine"}

        except Exception as e:
            logger.error(f"Error setting transform: {e}")
            return {"error": f"set_actor_transform failed: {e}"}
    
    # @mcp.tool()  # Level editing - disabled
    def get_actor_properties(
        ctx: Context,
        name: str,
        category_filter: str = "",
        include_inherited: bool = True
    ) -> Dict[str, Any]:
        """Get all reflected properties of an actor.

        Returns base info (name, class, transform) plus a 'properties' object with
        all UProperty values serialized to JSON via reflection.

        Args:
            name: Name of the actor to inspect
            category_filter: Optional category substring to filter properties (e.g. "Rendering", "Collision")
            include_inherited: Whether to include properties from parent classes (default true)
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"name": name}
            if category_filter:
                params["category_filter"] = category_filter
            if not include_inherited:
                params["include_inherited"] = False

            response = unreal.send_command("get_actor_properties", params)
            return response or {"error": "No response from Unreal Engine"}

        except Exception as e:
            logger.error(f"Error getting properties: {e}")
            return {"error": f"get_actor_properties failed: {e}"}

    # @mcp.tool()  # Level editing - disabled
    def set_actor_property(
        ctx: Context,
        name: str,
        property_name: str,
        property_value,
    ) -> Dict[str, Any]:
        """
        Set a property on an actor.
        
        Args:
            name: Name of the actor
            property_name: Name of the property to set
            property_value: Value to set the property to
            
        Returns:
            Dict containing response from Unreal with operation status
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}
                
            response = unreal.send_command("set_actor_property", {
                "name": name,
                "property_name": property_name,
                "property_value": property_value
            })
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}
            
            logger.info(f"Set actor property response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error setting actor property: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    # @mcp.tool() commented out because it's buggy
    def focus_viewport(
        ctx: Context,
        target: str = None,
        location: List[float] = None,
        distance: float = 1000.0,
        orientation: List[float] = None
    ) -> Dict[str, Any]:
        """
        Focus the viewport on a specific actor or location.
        
        Args:
            target: Name of the actor to focus on (if provided, location is ignored)
            location: [X, Y, Z] coordinates to focus on (used if target is None)
            distance: Distance from the target/location
            orientation: Optional [Pitch, Yaw, Roll] for the viewport camera
            
        Returns:
            Response from Unreal Engine
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}
                
            params = {}
            if target:
                params["target"] = target
            elif location:
                params["location"] = location
            
            if distance:
                params["distance"] = distance
                
            if orientation:
                params["orientation"] = orientation
                
            response = unreal.send_command("focus_viewport", params)
            return response or {"error": "No response from Unreal Engine"}

        except Exception as e:
            logger.error(f"Error focusing viewport: {e}")
            return {"error": f"focus_viewport failed: {e}"}

    # @mcp.tool()  # Level editing - disabled
    def spawn_blueprint_actor(
        ctx: Context,
        blueprint_name: str,
        actor_name: str,
        location: List[float] = [0.0, 0.0, 0.0],
        rotation: List[float] = [0.0, 0.0, 0.0]
    ) -> Dict[str, Any]:
        """Spawn an actor from a Blueprint.
        
        Args:
            ctx: The MCP context
            blueprint_name: Name of the Blueprint to spawn from
            actor_name: Name to give the spawned actor
            location: The [x, y, z] world location to spawn at
            rotation: The [pitch, yaw, roll] rotation in degrees
            
        Returns:
            Dict containing the spawned actor's properties
        """
        from unreal_mcp_server import get_unreal_connection
        
        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}
            
            # Ensure all parameters are properly formatted
            params = {
                "blueprint_name": blueprint_name,
                "actor_name": actor_name,
                "location": location or [0.0, 0.0, 0.0],
                "rotation": rotation or [0.0, 0.0, 0.0]
            }
            
            # Validate location and rotation formats
            for param_name in ["location", "rotation"]:
                param_value = params[param_name]
                if not isinstance(param_value, list) or len(param_value) != 3:
                    logger.error(f"Invalid {param_name} format: {param_value}. Must be a list of 3 float values.")
                    return {"error": f"Invalid {param_name} format. Must be a list of 3 float values."}
                # Ensure all values are float
                params[param_name] = [float(val) for val in param_value]
            
            logger.info(f"Spawning blueprint actor with params: {params}")
            response = unreal.send_command("spawn_blueprint_actor", params)
            
            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}
            
            logger.info(f"Spawn blueprint actor response: {response}")
            return response
            
        except Exception as e:
            error_msg = f"Error spawning blueprint actor: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def start_pie(ctx: Context) -> Dict[str, Any]:
        """Start a Play In Editor (PIE) session.

        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("start_pie", {})

            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error starting PIE: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def stop_pie(ctx: Context) -> Dict[str, Any]:
        """Stop the active Play In Editor (PIE) session.

        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("stop_pie", {})

            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error stopping PIE: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def get_pie_state(ctx: Context) -> Dict[str, Any]:
        """Get the current Play In Editor (PIE) state.

        Returns:
            Dict containing is_playing (bool) and is_paused (bool)
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("get_pie_state", {})

            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error getting PIE state: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    # -----------------------------------------------------------------------------
    # PIE input driving — start-and-poll wrappers around the C++ async ticker jobs.
    # The editor's TCP dispatch blocks the worker on the game thread; long-running
    # bodies would freeze the editor. The C++ side registers an FTSTicker per job
    # and returns immediately; these wrappers poll until the job reports done.
    # -----------------------------------------------------------------------------

    def _await_pie_job(unreal, job_id: str, expected_duration: float, sample_dt: float) -> Dict[str, Any]:
        """Poll pie_get_job_result until done or timeout. Cancels on timeout."""
        deadline = time.monotonic() + expected_duration + 5.0  # 5s safety margin
        poll_interval = max(sample_dt, 0.05)
        while time.monotonic() < deadline:
            time.sleep(poll_interval)
            response = unreal.send_command("pie_get_job_result", {"job_id": job_id})
            if not response:
                return {"error": "No response from Unreal during PIE job poll", "job_id": job_id}
            if response.get("status") == "error":
                return response
            if response.get("done"):
                return response
        # Timed out — try to cancel
        unreal.send_command("pie_cancel_job", {"job_id": job_id})
        return {"error": "PIE job timed out", "job_id": job_id, "expected_duration": expected_duration}

    @mcp.tool()
    def pie_drive_input(
        ctx: Context,
        duration_sec: float,
        direction_world: Optional[List[float]] = None,
        direction_named: Optional[str] = None,
        sample_dt_sec: float = 0.1,
        pin_yaw: Optional[float] = None,
        pin_pitch: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Drive AddMovementInput on the PIE player pawn for duration_sec, sampling state every sample_dt_sec.

        Pass exactly one of:
        - direction_world: [x, y, z] world-space vector (will be normalized).
        - direction_named: one of 'Forward', 'Back', 'Left', 'Right',
          'ForwardLeft', 'ForwardRight', 'BackLeft', 'BackRight'. Resolved per-frame
          relative to the current control rotation (yaw-only basis).

        Optional pin_yaw / pin_pitch lock the camera each tick (any combination).

        Returns dict with:
            done (bool), elapsed (float), duration (float),
            samples (list of {time, location:{x,y,z}, velocity:{x,y,z},
                              control_rotation:{pitch,yaw,roll},
                              gmc_movement_mode (str, e.g. 'Grounded'/'Airborne'/'Wallrun'),
                              active_tags (list[str])}),
            error (str, optional).
        """
        from unreal_mcp_server import get_unreal_connection

        if (direction_world is None) == (direction_named is None):
            return {"error": "Pass exactly one of direction_world or direction_named"}

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {
                "duration_sec": duration_sec,
                "sample_dt_sec": sample_dt_sec,
            }
            if direction_world is not None:
                params["direction_world"] = direction_world
            if direction_named is not None:
                params["direction_named"] = direction_named
            if pin_yaw is not None:
                params["pin_yaw"] = pin_yaw
            if pin_pitch is not None:
                params["pin_pitch"] = pin_pitch

            start = unreal.send_command("pie_drive_input_start", params)
            if not start:
                return {"error": "No response from pie_drive_input_start"}
            if start.get("status") == "error" or "job_id" not in start:
                return start

            return _await_pie_job(unreal, start["job_id"], duration_sec, sample_dt_sec)

        except Exception as e:
            error_msg = f"Error driving PIE input: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def pie_simulate_key(
        ctx: Context,
        action_name: str,
        pressed_for_seconds: float,
        sample_dt_sec: float = 0.05,
    ) -> Dict[str, Any]:
        """Hold an Enhanced Input action 'pressed' for pressed_for_seconds, sampling state.

        action_name resolves in this order:
        1. Full asset path starting with /Game/ (e.g. '/Game/Input/Actions/IA_Jump.IA_Jump').
        2. Project convention: '/Game/Input/Actions/IA_<Name>.IA_<Name>' (e.g. 'Jump' -> IA_Jump).
        3. Raw name: '/Game/Input/Actions/<Name>.<Name>'.

        Same return shape as pie_drive_input.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            start = unreal.send_command("pie_simulate_key_start", {
                "action_name": action_name,
                "pressed_for_seconds": pressed_for_seconds,
                "sample_dt_sec": sample_dt_sec,
            })
            if not start:
                return {"error": "No response from pie_simulate_key_start"}
            if start.get("status") == "error" or "job_id" not in start:
                return start

            return _await_pie_job(unreal, start["job_id"], pressed_for_seconds, sample_dt_sec)

        except Exception as e:
            error_msg = f"Error simulating PIE key: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def pie_set_control_rotation(
        ctx: Context,
        yaw: Optional[float] = None,
        pitch: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Set the player controller's control rotation in PIE. Provide at least one of yaw/pitch.

        One-shot; not pinned. To pin a rotation while driving input, pass pin_yaw/pin_pitch
        to pie_drive_input — those re-apply each frame.
        """
        from unreal_mcp_server import get_unreal_connection

        if yaw is None and pitch is None:
            return {"error": "Provide at least one of yaw or pitch"}

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params: Dict[str, Any] = {}
            if yaw is not None:
                params["yaw"] = yaw
            if pitch is not None:
                params["pitch"] = pitch

            response = unreal.send_command("pie_set_control_rotation", params)
            if not response:
                return {"error": "No response from pie_set_control_rotation"}
            return response

        except Exception as e:
            error_msg = f"Error setting PIE control rotation: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def take_screenshot(
        ctx: Context,
        filepath: str,
        source: str = "auto"
    ) -> Dict[str, Any]:
        """Take a screenshot of the viewport.

        Auto-detects whether PIE is running and captures from the appropriate viewport.

        Args:
            filepath: Path to save the PNG screenshot
            source: Viewport source - "auto" (game viewport if PIE active, else editor),
                    "editor" (always editor viewport), or "pie" (always game viewport)

        Returns:
            Dict containing filepath, source used, width, and height
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"filepath": filepath, "source": source}
            response = unreal.send_command("take_screenshot", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Screenshot saved: {response}")
            return response

        except Exception as e:
            error_msg = f"Error taking screenshot: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def execute_console_command(
        ctx: Context,
        command: str
    ) -> Dict[str, Any]:
        """Execute a console command in the Unreal Editor or PIE world.

        If PIE is active, the command runs in the PIE world. Otherwise it runs
        in the editor world.

        Args:
            command: The console command to execute (e.g., "stat fps", "slomo 0.5")

        Returns:
            Dict containing success status, the command executed, and which world it ran in
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            response = unreal.send_command("execute_console_command", {"command": command})

            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Console command response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error executing console command: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def get_editor_log(
        ctx: Context,
        lines: int = 100,
        severity: str = "",
        category: str = "",
        search: str = ""
    ) -> Dict[str, Any]:
        """Read recent lines from the Unreal Editor log with optional filtering.

        Args:
            lines: Number of most recent lines to return (default 100, max 5000)
            severity: Filter by severity - "Error", "Warning", or "WarningOrError"
            category: Filter by log category (e.g., "LogBlueprint", "LogCompile", "LogAngelscript")
            search: Free-text search filter within log lines

        Returns:
            Dict containing log_file path, total/matched/returned line counts, and lines array
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"lines": lines}
            if severity:
                params["severity"] = severity
            if category:
                params["category"] = category
            if search:
                params["search"] = search

            response = unreal.send_command("get_editor_log", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error getting editor log: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def execute_python(
        ctx: Context,
        code: str,
        mode: str = ""
    ) -> Dict[str, Any]:
        """Execute Python code inside the Unreal Editor via the Python Script Plugin.

        The full 'unreal' module is available for asset queries, property inspection,
        editor automation, etc. The execution environment persists across calls within
        the same editor session.

        Args:
            code: Python code to execute (single or multi-line)
            mode: Execution mode - "evaluate" to return a value, or "" for auto-detect

        Returns:
            Dict containing success status, result string, and log array with typed entries
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"code": code}
            if mode:
                params["mode"] = mode

            response = unreal.send_command("execute_python", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error executing Python: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def get_project_info(ctx: Context) -> Dict[str, Any]:
        """Get information about the currently connected Unreal Editor project.

        Returns the project name, project file path, and the MCP port the editor
        is listening on. Useful for multi-project orchestration and verifying
        which project the MCP bridge is connected to.

        Returns:
            Dict with project_name, project_path, port, and success fields
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Not connected to Unreal Engine"}

            response = unreal.send_command("get_project_info", {})
            if not response:
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            return {"error": f"Failed to get project info: {e}"}

    @mcp.tool()
    def launch_editor(
        ctx: Context,
        project_path: str = "",
        editor_path: str = ""
    ) -> Dict[str, Any]:
        """Launch the Unreal Editor as a detached process.

        Starts the editor in the background. Does not wait for it to finish.
        The MCP server will be able to connect once the editor finishes loading.

        Both arguments are required. Set defaults via the environment variables
        UNREAL_MCP_PROJECT_PATH and UNREAL_MCP_EDITOR_PATH so you don't have to
        pass them on every call.

        Args:
            project_path: Path to the .uproject file. Falls back to $UNREAL_MCP_PROJECT_PATH.
            editor_path:  Path to the UnrealEditor executable.
                          Falls back to $UNREAL_MCP_EDITOR_PATH.
        """
        if not project_path:
            project_path = os.environ.get("UNREAL_MCP_PROJECT_PATH", "")
        if not editor_path:
            editor_path = os.environ.get("UNREAL_MCP_EDITOR_PATH", "")
        if not project_path or not editor_path:
            return {
                "error": (
                    "launch_editor requires project_path and editor_path "
                    "(or UNREAL_MCP_PROJECT_PATH / UNREAL_MCP_EDITOR_PATH env vars)."
                )
            }
        import subprocess

        if not os.path.exists(editor_path):
            return {"error": f"Editor not found: {editor_path}"}
        if not os.path.exists(project_path):
            return {"error": f"Project not found: {project_path}"}

        try:
            proc = subprocess.Popen(
                [editor_path, project_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
            )
            return {
                "success": True,
                "pid": proc.pid,
                "editor": editor_path,
                "project": project_path
            }
        except Exception as e:
            return {"error": f"Failed to launch editor: {e}"}

    @mcp.tool()
    def close_editor(
        ctx: Context,
        save: bool = True
    ) -> Dict[str, Any]:
        """Save all dirty packages and close the Unreal Editor.

        Args:
            save: If true (default), save all modified assets before closing
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Not connected to Unreal Engine"}

            response = unreal.send_command("close_editor", {"save": save})
            return response or {"success": True, "message": "Close command sent"}

        except Exception as e:
            # Connection error is expected — editor is shutting down
            return {"success": True, "message": f"Editor closing (connection lost: {e})"}

    logger.info("Editor tools registered successfully")
