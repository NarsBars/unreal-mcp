"""
Enhanced Input Tools for Unreal MCP.

This module provides tools for creating and managing InputAction and
InputMappingContext assets in Unreal Engine's Enhanced Input system.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_input_tools(mcp: FastMCP):
    """Register enhanced input tools with the MCP server."""

    @mcp.tool()
    def create_input_action(
        ctx: Context,
        name: str,
        path: str = "/Game/Input",
        value_type: str = "Digital",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new Enhanced Input Action asset.

        Args:
            name: Name of the InputAction to create (e.g., "IA_SettingsMenu")
            path: Content browser path where the InputAction should be created
            value_type: Input value type - "Digital"/"bool", "Axis1D"/"float", "Axis2D"/"2d", "Axis3D"/"3d"
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("create_input_action", {
            "name": name,
            "path": path,
            "value_type": value_type,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def create_input_mapping_context(
        ctx: Context,
        name: str,
        path: str = "/Game/Input",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new Input Mapping Context asset.

        Args:
            name: Name of the InputMappingContext to create (e.g., "IMC_Default")
            path: Content browser path where the context should be created
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("create_input_mapping_context", {
            "name": name,
            "path": path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def add_input_mapping(
        ctx: Context,
        context_path: str,
        action_path: str,
        key: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a key mapping to an Input Mapping Context.

        Args:
            context_path: Full content path to the InputMappingContext (e.g., "/Game/Input/IMC_Default")
            action_path: Full content path to the InputAction (e.g., "/Game/Input/IA_SettingsMenu")
            key: Key name to bind (e.g., "Escape", "F1", "Gamepad_Special_Right")
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("add_input_mapping", {
            "context_path": context_path,
            "action_path": action_path,
            "key": key,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def get_input_info(
        ctx: Context,
        asset_path: str
    ) -> Dict[str, Any]:
        """
        Get information about an InputAction or InputMappingContext asset.

        Args:
            asset_path: Full content path to the input asset
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("get_input_info", {
            "asset_path": asset_path
        })
        return result or {"error": "No response from Unreal Engine"}
