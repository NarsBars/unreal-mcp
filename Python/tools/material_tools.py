"""
Material Tools for Unreal MCP.

This module provides tools for creating and editing materials, material instances,
material parameter collections, and material expressions in Unreal Engine.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")

def register_material_tools(mcp: FastMCP):
    """Register material tools with the MCP server."""

    @mcp.tool()
    def create_material(
        ctx: Context,
        name: str,
        path: str = "/Game/Materials",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new Material asset.

        Args:
            name: Name of the material to create
            path: Content browser path where the material should be created
            save: Whether to save the asset to disk (default True)

        Returns:
            Dict containing name and path of the created material
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"name": name, "path": path, "save": save}
            logger.info(f"Creating material: {params}")
            response = unreal.send_command("create_material", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Create material response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error creating material: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def create_material_instance(
        ctx: Context,
        name: str,
        parent: str,
        path: str = "/Game/Materials",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a Material Instance Constant from a parent material.

        Args:
            name: Name of the material instance to create
            parent: Content path of the parent material (e.g. "/Game/Materials/PP_Outline")
            path: Content browser path where the instance should be created
            save: Whether to save the asset to disk (default True)

        Returns:
            Dict containing name, path, and parent of the created instance
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"name": name, "parent": parent, "path": path, "save": save}
            logger.info(f"Creating material instance: {params}")
            response = unreal.send_command("create_material_instance", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Create material instance response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error creating material instance: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def create_material_parameter_collection(
        ctx: Context,
        name: str,
        path: str = "/Game/Materials",
        scalar_params: Optional[List[Dict[str, Any]]] = None,
        vector_params: Optional[List[Dict[str, Any]]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a Material Parameter Collection with scalar and vector parameters.

        Args:
            name: Name of the MPC to create
            path: Content browser path where the MPC should be created
            scalar_params: List of scalar parameters, each a dict with "name" and "default" keys.
                          Example: [{"name": "OutlineWidth", "default": 2.0}]
            vector_params: List of vector parameters, each a dict with "name" and "default" (RGBA array) keys.
                          Example: [{"name": "EnemyColor", "default": [1.0, 0.2, 0.15, 1.0]}]

        Returns:
            Dict containing name, path, and parameter counts
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"name": name, "path": path, "save": save}
            if scalar_params:
                params["scalar_params"] = scalar_params
            if vector_params:
                params["vector_params"] = vector_params

            logger.info(f"Creating MPC: {params}")
            response = unreal.send_command("create_material_parameter_collection", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Create MPC response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error creating MPC: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def add_material_expression(
        ctx: Context,
        material: str,
        expression_class: str,
        node_x: int = 0,
        node_y: int = 0,
        properties: Optional[Dict[str, Any]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a material expression node to a material.

        Args:
            material: Content path of the material (e.g. "/Game/Materials/PP_Outline")
            expression_class: Class name of the expression (e.g. "MaterialExpressionAdd",
                            "MaterialExpressionSceneTexture", "MaterialExpressionMultiply").
                            The "U" prefix is optional.
            node_x: X position of the node in the material graph
            node_y: Y position of the node in the material graph
            properties: Dict of expression-specific properties to set via reflection.
                       Example: {"SceneTextureId": 23} for CustomStencil on SceneTexture nodes.
                       For Custom HLSL nodes (MaterialExpressionCustom):
                         - "Code": HLSL code string
                         - "Description": Node caption
                         - "OutputType": 0=Float1, 1=Float2, 2=Float3, 3=Float4
                         - "Inputs": [{"InputName": "MyInput"}, ...] - named inputs for HLSL
                         - "AdditionalOutputs": [{"OutputName": "Out", "OutputType": 0}, ...]
                         - "IncludeFilePaths": ["/Engine/Private/MyInclude.ush", ...]
                       Supports int, float, bool, string, enum, struct, and array values.

        Returns:
            Dict containing expression_index (for use in connect commands),
            expression_class, and expression_name
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "expression_class": expression_class,
                "node_x": node_x,
                "node_y": node_y,
                "save": save
            }
            if properties:
                params["properties"] = properties

            logger.info(f"Adding material expression: {params}")
            response = unreal.send_command("add_material_expression", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Add material expression response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding material expression: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def connect_material_expressions(
        ctx: Context,
        material: str,
        from_expression_index: int,
        to_expression_index: int,
        from_output: str = "",
        to_input: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Connect two material expression nodes together.

        Args:
            material: Content path of the material
            from_expression_index: Index of the source expression (from add_material_expression)
            to_expression_index: Index of the destination expression
            from_output: Name of the output pin on the source (empty string for default/first output)
            to_input: Name of the input pin on the destination (empty string for default/first input).
                     For Custom HLSL nodes, use the InputName values set during creation
                     (e.g., to_input="FadeStart" if the node was created with
                     Inputs: [{"InputName": "FadeStart"}]).

        Returns:
            Dict containing connection status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "from_expression_index": from_expression_index,
                "to_expression_index": to_expression_index,
                "from_output": from_output,
                "to_input": to_input,
                "save": save
            }

            logger.info(f"Connecting material expressions: {params}")
            response = unreal.send_command("connect_material_expressions", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Connect expressions response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error connecting material expressions: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def connect_material_to_property(
        ctx: Context,
        material: str,
        from_expression_index: int,
        property: str,
        from_output: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Connect a material expression output to a material property input (e.g. EmissiveColor, Opacity).

        Args:
            material: Content path of the material
            from_expression_index: Index of the source expression
            property: Material property name. Valid values: EmissiveColor, Opacity, OpacityMask,
                     BaseColor, Metallic, Specular, Roughness, Normal, WorldPositionOffset,
                     AmbientOcclusion, Refraction, SubsurfaceColor, Anisotropy, Tangent,
                     PixelDepthOffset
            from_output: Name of the output pin on the source (empty for default)

        Returns:
            Dict containing connection status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "from_expression_index": from_expression_index,
                "property": property,
                "from_output": from_output,
                "save": save
            }

            logger.info(f"Connecting expression to material property: {params}")
            response = unreal.send_command("connect_material_to_property", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Connect to property response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error connecting to material property: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def set_material_property(
        ctx: Context,
        material: str,
        properties: Dict[str, Any] = {},
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set properties on a material (domain, blend mode, shading model, etc.) via reflection.

        Args:
            material: Content path of the material
            properties: Dict of property name -> value pairs. Common properties:
                       - "MaterialDomain": "MD_Surface" | "MD_PostProcess" | "MD_DeferredDecal" | "MD_UI" (use numeric 0-5 or string)
                       - "BlendMode": "BLEND_Opaque" | "BLEND_Translucent" | "BLEND_Masked" (use numeric 0-4 or string)
                       - "ShadingModel": Numeric value for shading model
                       - "bIsBlendable": true/false
                       - Any other UPROPERTY on UMaterial

        Returns:
            Dict containing count of properties set and any failures
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "properties": properties,
                "save": save
            }

            logger.info(f"Setting material properties: {params}")
            response = unreal.send_command("set_material_property", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Set material property response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting material properties: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def recompile_material(
        ctx: Context,
        material: str
    ) -> Dict[str, Any]:
        """
        Recompile a material after making changes to its expression graph.

        Args:
            material: Content path of the material to recompile

        Returns:
            Dict containing recompilation status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"material": material}
            logger.info(f"Recompiling material: {params}")
            response = unreal.send_command("recompile_material", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Recompile material response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error recompiling material: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def set_material_instance_scalar_parameter(
        ctx: Context,
        instance: str,
        parameter_name: str,
        value: float,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a scalar parameter override on a material instance.

        Args:
            instance: Content path of the material instance
            parameter_name: Name of the scalar parameter to set
            value: Float value to set

        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "instance": instance,
                "parameter_name": parameter_name,
                "value": value,
                "save": save
            }

            logger.info(f"Setting material instance scalar parameter: {params}")
            response = unreal.send_command("set_material_instance_scalar_parameter", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Set scalar parameter response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting material instance scalar parameter: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def set_material_instance_vector_parameter(
        ctx: Context,
        instance: str,
        parameter_name: str,
        value: List[float],
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a vector parameter override on a material instance.

        Args:
            instance: Content path of the material instance
            parameter_name: Name of the vector parameter to set
            value: Color as [R, G, B] or [R, G, B, A] array

        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "instance": instance,
                "parameter_name": parameter_name,
                "value": value,
                "save": save
            }

            logger.info(f"Setting material instance vector parameter: {params}")
            response = unreal.send_command("set_material_instance_vector_parameter", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Set vector parameter response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting material instance vector parameter: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def set_material_instance_texture_parameter(
        ctx: Context,
        instance: str,
        parameter_name: str,
        texture_path: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a texture parameter override on a material instance.

        Args:
            instance: Content path of the material instance
            parameter_name: Name of the texture parameter to set
            texture_path: Content path of the texture to assign

        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "instance": instance,
                "parameter_name": parameter_name,
                "texture_path": texture_path,
                "save": save
            }

            logger.info(f"Setting material instance texture parameter: {params}")
            response = unreal.send_command("set_material_instance_texture_parameter", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Set texture parameter response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting material instance texture parameter: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def set_material_instance_static_switch_parameter(
        ctx: Context,
        instance: str,
        parameter_name: str,
        value: bool,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a static switch parameter override on a material instance.

        Args:
            instance: Content path of the material instance
            parameter_name: Name of the static switch parameter to set
            value: Boolean value (true/false)

        Returns:
            Dict containing success status
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "instance": instance,
                "parameter_name": parameter_name,
                "value": value,
                "save": save
            }

            logger.info(f"Setting material instance static switch parameter: {params}")
            response = unreal.send_command("set_material_instance_static_switch_parameter", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Set static switch parameter response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting material instance static switch parameter: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def get_material_info(
        ctx: Context,
        material: str
    ) -> Dict[str, Any]:
        """
        Get detailed information about a material's expression graph, including all nodes,
        their types, parameter names, input connections, and what's connected to each
        material property pin (BaseColor, Roughness, etc.).

        Use this to inspect an existing material before making modifications.

        Args:
            material: Content path of the material (e.g. "/Game/Materials/Characters/M_Gear_Master")

        Returns:
            Dict containing:
            - expression_count: Total number of expressions
            - expressions: Array of expression objects with index, class, name, desc,
              parameter_name (for parameters), default_value, and inputs (connections)
            - property_connections: Object mapping property names (BaseColor, Roughness, etc.)
              to the expression index and output index connected to each
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"material": material}
            logger.info(f"Getting material info: {params}")
            response = unreal.send_command("get_material_info", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Get material info response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting material info: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def get_custom_expression_code(
        ctx: Context,
        material: str,
        expression_index: int
    ) -> Dict[str, Any]:
        """
        Get the HLSL code from a Custom material expression node.

        Returns the full HLSL source code, description, output type, input names,
        additional outputs, and include file paths for a MaterialExpressionCustom node.

        Args:
            material: Content path of the material (e.g. "/Game/Materials/PostProcess/PP_Outline")
            expression_index: Index of the Custom expression node (from get_material_info)

        Returns:
            Dict containing:
            - code: The HLSL source code string
            - description: Node caption/description
            - output_type: Output type enum (0=Float1, 1=Float2, 2=Float3, 3=Float4)
            - inputs: Array of input pin names
            - additional_outputs: Array of additional output pins
            - include_file_paths: Array of included .ush/.usf files
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"material": material, "expression_index": expression_index}
            logger.info(f"Getting custom expression code: {params}")
            response = unreal.send_command("get_custom_expression_code", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Get custom expression code response received (code length: {len(response.get('code', ''))})")
            return response

        except Exception as e:
            error_msg = f"Error getting custom expression code: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def set_custom_expression_code(
        ctx: Context,
        material: str,
        expression_index: int,
        code: str,
        description: Optional[str] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set the HLSL code on a Custom material expression node.

        Replaces the entire HLSL code of a MaterialExpressionCustom node.
        Triggers pin rebuild and marks the material dirty for recompilation.

        Args:
            material: Content path of the material (e.g. "/Game/Materials/PostProcess/PP_Outline")
            expression_index: Index of the Custom expression node (from get_material_info)
            code: The new HLSL source code to set
            description: Optional new description/caption for the node
            save: Whether to save the material after modification (default True)

        Returns:
            Dict containing success status and code length
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "expression_index": expression_index,
                "code": code,
                "save": save
            }
            if description is not None:
                params["description"] = description

            logger.info(f"Setting custom expression code on {material}[{expression_index}] ({len(code)} chars)")
            response = unreal.send_command("set_custom_expression_code", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Set custom expression code response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting custom expression code: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def get_expression_properties(
        ctx: Context,
        material: str,
        expression_index: int
    ) -> Dict[str, Any]:
        """
        Get all editable properties on a material expression node via reflection.

        Returns every UPROPERTY(EditAnywhere) on the expression, including type-specific
        properties like SamplerType on texture nodes, ParameterName on parameters, etc.

        Args:
            material: Content path of the material (e.g. "/Game/Materials/Characters/M_Hair_Master")
            expression_index: Index of the expression node (from get_material_info)

        Returns:
            Dict containing expression_class, expression_name, and properties array.
            Each property has: name, type, value, category, is_base (if from base class).
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {"material": material, "expression_index": expression_index}
            logger.info(f"Getting expression properties: {material}[{expression_index}]")
            response = unreal.send_command("get_expression_properties", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error getting expression properties: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def set_expression_property(
        ctx: Context,
        material: str,
        expression_index: int,
        properties: Dict[str, Any],
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set properties on an existing material expression node via reflection.

        Can modify any UPROPERTY on the expression, such as:
        - SamplerType on TextureSampleParameter2D (0=Color, 1=Grayscale, etc.)
        - ParameterName on any parameter expression
        - DefaultValue on scalar/vector parameters
        - Any other reflection-accessible property

        Triggers PostEditChangeProperty after setting to rebuild pins and update visuals.

        Args:
            material: Content path of the material
            expression_index: Index of the expression node (from get_material_info)
            properties: Dict of property name -> value pairs to set
            save: Whether to save the material after modification (default True)

        Returns:
            Dict containing properties_set and properties_failed arrays
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "expression_index": expression_index,
                "properties": properties,
                "save": save
            }
            logger.info(f"Setting expression property: {material}[{expression_index}] = {properties}")
            response = unreal.send_command("set_expression_property", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error setting expression property: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def disconnect_expression(
        ctx: Context,
        material: str,
        expression_index: int,
        input_name: Optional[str] = None,
        input_index: Optional[int] = None,
        disconnect_all: bool = False,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Disconnect input pin(s) on a material expression node.

        Three modes of operation (specify exactly one):
        - disconnect_all=True: Clear ALL input connections on the expression
        - input_index=N: Clear a specific input by its index
        - input_name="A": Clear a specific input by its name

        Note: This only disconnects INPUTS on the target node. It does NOT disconnect
        other nodes that use this node as their source. For full disconnection,
        use remove_expression (which auto-disconnects all links).

        Args:
            material: Content path of the material
            expression_index: Index of the expression node
            input_name: Name of the input pin to disconnect (e.g. "A", "B", "Coordinates")
            input_index: Index of the input pin to disconnect
            disconnect_all: If True, disconnect all inputs on this expression
            save: Whether to save after modification (default True)

        Returns:
            Dict containing disconnected_count
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "expression_index": expression_index,
                "save": save
            }
            if input_name is not None:
                params["input_name"] = input_name
            if input_index is not None:
                params["input_index"] = input_index
            if disconnect_all:
                params["disconnect_all"] = True

            logger.info(f"Disconnecting expression: {material}[{expression_index}]")
            response = unreal.send_command("disconnect_expression", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            return response

        except Exception as e:
            error_msg = f"Error disconnecting expression: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    @mcp.tool()
    def remove_expression(
        ctx: Context,
        material: str,
        expression_index: int,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Delete a material expression node from the material graph.

        Automatically disconnects all links to/from this expression before removing it.
        Uses UMaterialEditingLibrary::DeleteMaterialExpression which handles:
        - Breaking all input/output connections
        - Clearing material property connections (BaseColor, Roughness, etc.)
        - Removing parameter registration
        - Garbage collection marking

        IMPORTANT: After removal, all expression indices above the removed index
        shift down by 1. You MUST re-query get_material_info before performing
        any further index-based operations on this material.

        Args:
            material: Content path of the material
            expression_index: Index of the expression node to remove
            save: Whether to save after modification (default True)

        Returns:
            Dict containing removed_index, removed_class, removed_name, new_expression_count
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                return {"error": "Failed to connect to Unreal Engine"}

            params = {
                "material": material,
                "expression_index": expression_index,
                "save": save
            }
            logger.info(f"Removing expression: {material}[{expression_index}]")
            response = unreal.send_command("remove_expression", params)

            if not response:
                return {"error": "No response from Unreal Engine"}

            logger.info(f"Remove expression response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error removing expression: {e}"
            logger.error(error_msg)
            return {"error": error_msg}

    logger.info("Material tools registered successfully")
