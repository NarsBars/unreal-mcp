"""
Blueprint Tools for Unreal MCP.

Tools for creating and manipulating Blueprint assets in Unreal Engine.
"""

import logging
from typing import Dict, List, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def _send(command: str, params: dict) -> Dict[str, Any]:
    """Helper to send a command to Unreal and return the result."""
    from unreal_mcp_server import get_unreal_connection
    conn = get_unreal_connection()
    if not conn:
        return {"error": "Not connected to Unreal Engine"}
    result = conn.send_command(command, params)
    return result or {"error": "No response from Unreal Engine"}

def register_blueprint_tools(mcp: FastMCP):
    """Register Blueprint tools with the MCP server."""

    @mcp.tool()
    def create_blueprint(
        ctx: Context,
        name: str,
        parent_class: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """Create a new Blueprint class.

        Args:
            name: Name of the Blueprint to create
            parent_class: Parent class (e.g. "Actor", "Pawn", "Character")
            save: Whether to save the asset to disk
        """
        return _send("create_blueprint", {
            "name": name,
            "parent_class": parent_class,
            "save": save
        })

    @mcp.tool()
    def add_component_to_blueprint(
        ctx: Context,
        blueprint_name: str,
        component_type: str,
        component_name: str,
        parent_component: str = "",
        location: List[float] = [],
        rotation: List[float] = [],
        scale: List[float] = [],
        component_properties: Dict[str, Any] = {},
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a component to a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            component_type: Type of component to add (class name without U prefix)
            component_name: Name for the new component
            parent_component: Optional parent component name to attach to. If empty, adds as root.
            location: [X, Y, Z] coordinates for component's position
            rotation: [Pitch, Yaw, Roll] values for component's rotation
            scale: [X, Y, Z] values for component's scale
            component_properties: Additional properties to set on the component
        """
        params = {
            "blueprint_name": blueprint_name,
            "component_type": component_type,
            "component_name": component_name,
            "location": location or [0.0, 0.0, 0.0],
            "rotation": rotation or [0.0, 0.0, 0.0],
            "scale": scale or [1.0, 1.0, 1.0],
            "save": save
        }
        if parent_component:
            params["parent_component"] = parent_component
        if component_properties:
            params["component_properties"] = component_properties

        # Validate vector formats
        for param_name in ["location", "rotation", "scale"]:
            param_value = params[param_name]
            if not isinstance(param_value, list) or len(param_value) != 3:
                return {"error": f"Invalid {param_name} format. Must be a list of 3 float values."}
            params[param_name] = [float(val) for val in param_value]

        return _send("add_component_to_blueprint", params)

    @mcp.tool()
    def set_component_property(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        property_name: str,
        property_value,
        save: bool = True
    ) -> Dict[str, Any]:
        """Set a property on a component in a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the component
            property_name: Name of the property to set
            property_value: Value to set the property to
        """
        return _send("set_component_property", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "property_name": property_name,
            "property_value": property_value,
            "save": save
        })

    @mcp.tool()
    def compile_blueprint(
        ctx: Context,
        blueprint_name: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """Compile a Blueprint.

        Args:
            blueprint_name: Name of the Blueprint to compile
            save: Whether to save after compilation
        """
        return _send("compile_blueprint", {
            "blueprint_name": blueprint_name,
            "save": save
        })

    @mcp.tool()
    def set_blueprint_property(
        ctx: Context,
        blueprint_name: str,
        property_name: str,
        property_value,
        save: bool = True
    ) -> Dict[str, Any]:
        """Set a property on a Blueprint's class default object.

        Args:
            blueprint_name: Name of the target Blueprint
            property_name: Name of the property to set
            property_value: Value to set the property to
        """
        return _send("set_blueprint_property", {
            "blueprint_name": blueprint_name,
            "property_name": property_name,
            "property_value": property_value,
            "save": save
        })

    logger.info("Blueprint tools registered successfully")
