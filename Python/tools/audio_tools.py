"""
Audio Tools for Unreal MCP.

This module provides tools for creating and managing SoundClass and SoundMix
assets in Unreal Engine.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_audio_tools(mcp: FastMCP):
    """Register audio tools with the MCP server."""

    @mcp.tool()
    def create_sound_class(
        ctx: Context,
        name: str,
        path: str = "/Game/Audio",
        parent_class: str = "",
        volume: float = 1.0,
        pitch: float = 1.0,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new SoundClass asset.

        Args:
            name: Name of the SoundClass to create
            path: Content browser path where the SoundClass should be created
            parent_class: Optional full path to parent SoundClass (e.g., "/Game/Audio/SC_Master")
            volume: Default volume (0.0 - 1.0, default 1.0)
            pitch: Default pitch (default 1.0)
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
        if parent_class:
            params["parent_class"] = parent_class

        properties = {}
        if volume != 1.0:
            properties["volume"] = volume
        if pitch != 1.0:
            properties["pitch"] = pitch
        if properties:
            params["properties"] = properties

        result = conn.send_command("create_sound_class", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def create_sound_mix(
        ctx: Context,
        name: str,
        path: str = "/Game/Audio",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new SoundMix asset.

        Args:
            name: Name of the SoundMix to create
            path: Content browser path where the SoundMix should be created
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("create_sound_mix", {
            "name": name,
            "path": path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def set_sound_class_parent(
        ctx: Context,
        asset_path: str,
        parent_path: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set the parent class on a SoundClass asset.

        Args:
            asset_path: Full content path to the SoundClass (e.g., "/Game/Audio/SC_Music")
            parent_path: Full content path to the parent SoundClass (e.g., "/Game/Audio/SC_Master")
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("set_sound_class_parent", {
            "asset_path": asset_path,
            "parent_path": parent_path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def get_audio_info(
        ctx: Context,
        asset_path: str
    ) -> Dict[str, Any]:
        """
        Get information about a SoundClass or SoundMix asset.

        Args:
            asset_path: Full content path to the audio asset
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("get_audio_info", {
            "asset_path": asset_path
        })
        return result or {"error": "No response from Unreal Engine"}
