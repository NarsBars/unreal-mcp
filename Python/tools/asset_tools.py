"""
Asset Management Tools for Unreal MCP.

This module provides tools for searching, importing, duplicating,
renaming, moving, and deleting assets in the Unreal Engine content browser.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

logger = logging.getLogger("UnrealMCP")

def register_asset_tools(mcp: FastMCP):
    """Register asset management tools with the MCP server."""

    @mcp.tool()
    def search_assets(
        ctx: Context,
        query: str = "",
        class_filter: str = "",
        path: str = "/Game",
        recursive: bool = True,
        limit: int = 100
    ) -> Dict[str, Any]:
        """
        Search for assets in the content browser.

        Args:
            query: Optional name substring to filter by
            class_filter: Optional class name to filter by (e.g., "StaticMesh", "Blueprint", "Material")
            path: Content path to search in (default: "/Game")
            recursive: Whether to search subdirectories (default: True)
            limit: Maximum number of results to return (default: 100)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {"path": path, "recursive": recursive, "limit": limit}
        if query:
            params["query"] = query
        if class_filter:
            params["class_filter"] = class_filter

        result = conn.send_command("search_assets", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def import_asset(
        ctx: Context,
        source_path: str,
        destination_path: str,
        asset_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Import an external file into the Unreal project.

        Args:
            source_path: Filesystem path to the source file (e.g., "C:/Models/chair.fbx")
            destination_path: Content browser path (e.g., "/Game/Meshes")
            asset_name: Optional name for the imported asset
            save: Whether to save after import (default: True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {"source_path": source_path, "destination_path": destination_path, "save": save}
        if asset_name:
            params["asset_name"] = asset_name

        result = conn.send_command("import_asset", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def duplicate_asset(
        ctx: Context,
        source_path: str,
        destination_path: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Duplicate an existing asset to a new path.

        Args:
            source_path: Full content path of the source asset (e.g., "/Game/Materials/M_Base")
            destination_path: Full content path for the duplicate (e.g., "/Game/Materials/M_Base_Copy")
            save: Whether to save the duplicated asset (default: True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("duplicate_asset", {
            "source_path": source_path,
            "destination_path": destination_path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def rename_asset(
        ctx: Context,
        source_path: str,
        new_name: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Rename an asset (keeps it in the same directory).

        Args:
            source_path: Full content path of the asset to rename
            new_name: New name for the asset
            save: Whether to save after renaming (default: True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("rename_asset", {
            "source_path": source_path,
            "new_name": new_name,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def move_asset(
        ctx: Context,
        source_path: str,
        destination_path: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Move an asset to a new location in the content browser.

        Args:
            source_path: Full content path of the asset to move
            destination_path: Full content path of the new location (including asset name)
            save: Whether to save after moving (default: True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("move_asset", {
            "source_path": source_path,
            "destination_path": destination_path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def delete_asset(
        ctx: Context,
        asset_path: str,
        force: bool = False
    ) -> Dict[str, Any]:
        """
        Delete an asset from the content browser.

        Args:
            asset_path: Full content path of the asset to delete
            force: Force delete even if asset has references (default: False)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("delete_asset", {
            "asset_path": asset_path,
            "force": force
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def get_asset_dependencies(
        ctx: Context,
        asset_path: str,
        recursive: bool = False
    ) -> Dict[str, Any]:
        """
        Get the dependencies of an asset.

        Args:
            asset_path: Full content path of the asset
            recursive: Whether to get recursive dependencies (default: False)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("get_asset_dependencies", {
            "asset_path": asset_path,
            "recursive": recursive
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def save_asset(
        ctx: Context,
        asset_path: str
    ) -> Dict[str, Any]:
        """
        Force-save an asset to disk.

        Args:
            asset_path: Full content path of the asset to save
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("save_asset", {
            "asset_path": asset_path
        })
        return result or {"error": "No response from Unreal Engine"}
