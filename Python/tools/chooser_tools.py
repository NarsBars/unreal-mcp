"""
Chooser Table Tools for Unreal MCP.

This module provides tools for reading and modifying UChooserTable assets,
including nested sub-tables, column definitions, bindings, and row values.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_chooser_tools(mcp: FastMCP):
    """Register chooser table tools with the MCP server."""

    @mcp.tool()
    def read_chooser_table(
        ctx: Context,
        asset_path: str,
        sub_table: str = ""
    ) -> Dict[str, Any]:
        """
        Read a ChooserTable's structure — columns, bindings, row values, and nested tables.

        Supports navigating into nested sub-tables with dot-separated paths.
        Returns column types, their input bindings, and per-row cell values.

        Args:
            asset_path: Full content path of the ChooserTable (e.g., "/Game/Animations/CHT_LocomotionDatabase")
            sub_table: Optional dot-separated path to a nested sub-table (e.g., "Stand Walks.Stand Walks F")
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {"asset_path": asset_path}
        if sub_table:
            params["sub_table"] = sub_table

        result = conn.send_command("read_chooser_table", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def set_chooser_column_value(
        ctx: Context,
        asset_path: str,
        column_index: int,
        row_index: int,
        value: Any,
        sub_table: str = "",
        comparison: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a cell value in a ChooserTable column.

        Args:
            asset_path: Full content path of the ChooserTable
            column_index: Index of the column to modify
            row_index: Index of the row to modify
            value: The value to set. Type depends on column:
                   - MultiEnum: integer bitmask
                   - Enum: integer value or {"value": int, "comparison": str}
                   - Bool: "MatchTrue", "MatchFalse", "MatchAny"
                   - FloatRange: {"min": float, "max": float, "no_min": bool, "no_max": bool}
                   - GameplayTag: tag string
            sub_table: Optional dot-separated path to a nested sub-table
            comparison: Optional comparison type for Enum columns ("MatchEqual", "MatchNotEqual", "MatchAny")
            save: Whether to save after modification (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "asset_path": asset_path,
            "column_index": column_index,
            "row_index": row_index,
            "value": value,
            "save": save
        }
        if sub_table:
            params["sub_table"] = sub_table
        if comparison:
            params["comparison"] = comparison

        result = conn.send_command("set_chooser_column_value", params)
        return result or {"error": "No response from Unreal Engine"}
