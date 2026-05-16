"""
Niagara/VFX Tools for Unreal MCP.

This module provides tools for creating Niagara systems and emitters.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_niagara_tools(mcp: FastMCP):
    """Register Niagara/VFX tools with the MCP server."""

    @mcp.tool()
    def create_niagara_system(
        ctx: Context,
        name: str,
        path: str = "/Game/VFX",
        template: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new Niagara System asset.

        Args:
            name: Name of the Niagara System to create
            path: Content browser path where the system should be created
            template: Optional full content path to a template system to copy from
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "name": name,
            "path": path,
            "save": save
        }
        if template:
            params["template"] = template

        result = conn.send_command("create_niagara_system", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def create_niagara_emitter(
        ctx: Context,
        name: str,
        path: str = "/Game/VFX",
        template: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new Niagara Emitter asset.

        Args:
            name: Name of the Niagara Emitter to create
            path: Content browser path where the emitter should be created
            template: Optional full content path to a template emitter to copy from
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "name": name,
            "path": path,
            "save": save
        }
        if template:
            params["template"] = template

        result = conn.send_command("create_niagara_emitter", params)
        return result or {"error": "No response from Unreal Engine"}
