"""
Data Asset Tools for Unreal MCP.

This module provides tools for creating and managing UDataAsset instances
(including AngelScript subclasses), setting complex property types
(FGameplayTag, TArray, TSubclassOf, etc.), and reading back property values.
"""

import logging
from typing import Dict, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_data_asset_tools(mcp: FastMCP):
    """Register data asset tools with the MCP server."""

    @mcp.tool()
    def create_data_asset(
        ctx: Context,
        asset_name: str,
        asset_path: str,
        parent_class: str,
        properties: Optional[Dict[str, Any]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new DataAsset instance of any UDataAsset subclass.

        Works with both native C++ classes and AngelScript-defined classes.
        Supports setting complex property types including FGameplayTag,
        FGameplayTagContainer, TArray, TSubclassOf, FText, FVector, and
        nested structs.

        Args:
            asset_name: Name of the asset to create (e.g., "PD_SteadyAim")
            asset_path: Content browser path (e.g., "/Game/Data/Perks/Bow")
            parent_class: Class name or path (e.g., "UPerkDefinition", "PerkDefinition")
            properties: Optional dict of property name -> value to set on creation.
                       Supported value types:
                       - bool, int, float, str (basic types)
                       - str for FGameplayTag (e.g., "Perk.Bow.Hunter.SteadyAim")
                       - str for FText (auto-converted)
                       - str for FName
                       - list of str for FGameplayTagContainer (e.g., ["Tag.A", "Tag.B"])
                       - list for TArray (elements match inner type)
                       - str for TSubclassOf (class path e.g., "/Script/Module.ClassName")
                       - str for UObject* ref (asset path e.g., "/Game/Path/Asset.Asset")
                       - dict for FVector ({"X":1,"Y":2,"Z":3})
                       - dict for FRotator ({"Pitch":0,"Yaw":90,"Roll":0})
                       - list of [R,G,B,A] for FLinearColor
                       - dict for nested structs ({"Field1": val, "Field2": val})
            save: Whether to save after creation (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "asset_name": asset_name,
            "asset_path": asset_path,
            "parent_class": parent_class,
            "save": save
        }
        if properties:
            params["properties"] = properties

        result = conn.send_command("create_data_asset", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def create_asset(
        ctx: Context,
        asset_name: str,
        asset_path: str,
        asset_class: str,
        properties: Optional[Dict[str, Any]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create any UObject-based asset (SubsurfaceProfile, CurveFloat, PhysicalMaterial, etc.).

        Unlike create_data_asset (which only accepts UDataAsset subclasses), this command
        works with any UObject class that has an engine-registered factory.

        Args:
            asset_name: Name of the asset (e.g., "SSP_Skin")
            asset_path: Content browser path (e.g., "/Game/Materials/Characters")
            asset_class: Class name (e.g., "SubsurfaceProfile", "CurveFloat", "PhysicalMaterial")
            properties: Optional dict of property name -> value to set on creation.
                       Supports all types from create_data_asset (bool, int, float, str,
                       FGameplayTag, TArray, FVector, FLinearColor, nested structs, etc.)
            save: Whether to save after creation (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "asset_name": asset_name,
            "asset_path": asset_path,
            "asset_class": asset_class,
            "save": save
        }
        if properties:
            params["properties"] = properties

        result = conn.send_command("create_asset", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def set_data_asset_property(
        ctx: Context,
        asset_path: str,
        property_name: str,
        property_value: Any,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a property on an existing DataAsset.

        Supports all complex property types (FGameplayTag, TArray,
        TSubclassOf, nested structs, etc.).

        Args:
            asset_path: Full content path of the asset (e.g., "/Game/Data/Perks/Bow/PD_SteadyAim")
            property_name: Name of the property to set (e.g., "PerkTag", "Modifiers")
            property_value: Value to set. Type depends on the property:
                           - str for FGameplayTag (e.g., "Perk.Bow.Hunter.SteadyAim")
                           - list of str for FGameplayTagContainer
                           - list of dicts for TArray<FStruct>
                           - str for TSubclassOf (class path)
                           - dict for nested structs
            save: Whether to save after modification (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("set_data_asset_property", {
            "asset_path": asset_path,
            "property_name": property_name,
            "property_value": property_value,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def get_data_asset_properties(
        ctx: Context,
        asset_path: str
    ) -> Dict[str, Any]:
        """
        Read all properties from a DataAsset.

        Returns all subclass-specific properties (skips base UDataAsset/UObject properties).

        Args:
            asset_path: Full content path of the asset (e.g., "/Game/Data/Perks/Bow/PD_SteadyAim")
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("get_data_asset_properties", {
            "asset_path": asset_path
        })
        return result or {"error": "No response from Unreal Engine"}

    # ========================================================================
    # Array element operations
    # ========================================================================

    @mcp.tool()
    def get_array_element(
        ctx: Context,
        asset_path: str,
        property_name: str,
        index: int
    ) -> Dict[str, Any]:
        """
        Read a single element from a data asset array property by index.

        Returns the element as structured JSON (not opaque ExportText).

        Args:
            asset_path: Full content path (e.g., "/Game/AbilitySystem/AttributeData")
            property_name: Name of the array property (e.g., "AttributeData")
            index: Zero-based index of the element to read
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("get_array_element", {
            "asset_path": asset_path,
            "property_name": property_name,
            "index": index
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def set_array_element(
        ctx: Context,
        asset_path: str,
        property_name: str,
        index: int,
        value: Any,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Write a single element in a data asset array property by index.

        Args:
            asset_path: Full content path (e.g., "/Game/AbilitySystem/AttributeData")
            property_name: Name of the array property (e.g., "AttributeData")
            index: Zero-based index of the element to overwrite
            value: JSON object matching the element's struct layout
            save: Whether to save after modification (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("set_array_element", {
            "asset_path": asset_path,
            "property_name": property_name,
            "index": index,
            "value": value,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def add_array_element(
        ctx: Context,
        asset_path: str,
        property_name: str,
        value: Any,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Append element(s) to a data asset array property.

        Args:
            asset_path: Full content path (e.g., "/Game/AbilitySystem/AttributeData")
            property_name: Name of the array property (e.g., "AttributeData")
            value: Single JSON object to append, or array of objects for batch append.
                   Example for GMCAttributesData:
                   {"AttributeTag": "Attribute.Power.Weapon", "DefaultValue": 0.0,
                    "Clamp": {"Min": 0.0, "MinAttributeTag": "None", "Max": 0.0, "MaxAttributeTag": "None"},
                    "bGMCBound": false}
            save: Whether to save after modification (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("add_array_element", {
            "asset_path": asset_path,
            "property_name": property_name,
            "value": value,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def remove_array_element(
        ctx: Context,
        asset_path: str,
        property_name: str,
        index: int,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Remove an element from a data asset array property by index.

        WARNING: This shifts all subsequent elements down. If you are removing
        multiple elements, remove from highest index to lowest to preserve indices.

        Args:
            asset_path: Full content path (e.g., "/Game/AbilitySystem/AttributeData")
            property_name: Name of the array property (e.g., "AttributeData")
            index: Zero-based index of the element to remove
            save: Whether to save after modification (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("remove_array_element", {
            "asset_path": asset_path,
            "property_name": property_name,
            "index": index,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def get_array_length(
        ctx: Context,
        asset_path: str,
        property_name: str
    ) -> Dict[str, Any]:
        """
        Get the number of elements in a data asset array property.

        Args:
            asset_path: Full content path (e.g., "/Game/AbilitySystem/AttributeData")
            property_name: Name of the array property (e.g., "AttributeData")
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("get_array_length", {
            "asset_path": asset_path,
            "property_name": property_name
        })
        return result or {"error": "No response from Unreal Engine"}

    # ========================================================================
    # Raw property text import (for InstancedStruct arrays, Chooser Tables, etc.)
    # ========================================================================

    @mcp.tool()
    def import_property_text(
        ctx: Context,
        asset_path: str,
        property_name: str,
        text: str,
        sub_object_path: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a property using its T3D text representation via FProperty::ImportText.

        This handles complex types that can't be set via JSON, such as
        TArray<FInstancedStruct> on Chooser Tables. The text format is the same
        as what appears in .t3d exports for that property.

        Args:
            asset_path: Full content path of the asset (e.g., "/Game/Path/CHT_MyTable")
            property_name: Name of the property to set (e.g., "ColumnsStructs")
            text: The T3D text value for the property (e.g., the full ColumnsStructs(...) text)
            sub_object_path: Optional sub-object name within the asset (e.g., "Stand Idles")
            save: Whether to save after modification (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("import_property_text", {
            "asset_path": asset_path,
            "property_name": property_name,
            "text": text,
            "sub_object_path": sub_object_path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    # ========================================================================
    # Convenience: find array element by field value (Python-only composition)
    # ========================================================================

    @mcp.tool()
    def find_array_element(
        ctx: Context,
        asset_path: str,
        property_name: str,
        field_name: str,
        field_value: str,
        partial_match: bool = False
    ) -> Dict[str, Any]:
        """
        Find array element(s) where a field matches a value. Returns index + element.

        This is a Python-only composition tool that reads the full array and searches it.
        Works with any array-of-structs property.

        Examples:
            # Find specific attribute by tag
            find_array_element("/Game/AbilitySystem/AttributeData", "AttributeData",
                              "AttributeTag", "Attribute.Health.Max")

            # Find all movement attributes (partial match)
            find_array_element("/Game/AbilitySystem/AttributeData", "AttributeData",
                              "AttributeTag", "Attribute.Movement", partial_match=True)

        Args:
            asset_path: Full content path of the asset
            property_name: Name of the array property to search
            field_name: Name of the struct field to match against
            field_value: Value to search for (string comparison)
            partial_match: If True, matches when field_value is contained in the field.
                          Returns all matches. If False, returns first exact match only.
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        # Read the full asset properties
        response = conn.send_command("get_data_asset_properties", {
            "asset_path": asset_path
        })
        if not response or response.get("status") == "error":
            return response or {"error": "Failed to read asset properties"}

        # Unwrap the TCP response envelope: {"status": "success", "result": {...}}
        result = response.get("result", response)
        if not result.get("success"):
            return result

        properties = result.get("properties", {})
        array_data = properties.get(property_name)
        if array_data is None:
            return {"error": f"Property '{property_name}' not found on asset"}
        if not isinstance(array_data, list):
            return {"error": f"Property '{property_name}' is not an array"}

        matches = []
        for i, element in enumerate(array_data):
            if not isinstance(element, dict):
                continue
            elem_value = element.get(field_name)
            if elem_value is None:
                continue
            elem_str = str(elem_value)
            if partial_match:
                if field_value in elem_str:
                    matches.append({"index": i, "element": element})
            else:
                if elem_str == field_value:
                    return {"success": True, "index": i, "element": element}

        if partial_match:
            return {"success": True, "match_count": len(matches), "matches": matches}

        return {"success": False, "error": f"No element found where {field_name} == '{field_value}'"}
