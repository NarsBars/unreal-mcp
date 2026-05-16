"""
Blueprint Node Tools for Unreal MCP.

Tools for manipulating Blueprint graph nodes and connections.
"""

import logging
from typing import Dict, List, Any, Optional
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

def register_blueprint_node_tools(mcp: FastMCP):
    """Register Blueprint node manipulation tools with the MCP server."""

    @mcp.tool()
    def add_blueprint_event_node(
        ctx: Context,
        blueprint_name: str,
        event_name: str,
        node_position = None,
        graph_name: str = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add an event node to a Blueprint's graph.

        Args:
            blueprint_name: Name of the target Blueprint
            event_name: Name of the event. Use 'Receive' prefix for standard events:
                       - 'ReceiveBeginPlay' for Begin Play
                       - 'ReceiveTick' for Tick
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name (defaults to EventGraph)
        """
        params = {
            "blueprint_name": blueprint_name,
            "event_name": event_name,
            "node_position": node_position or [0, 0],
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        return _send("add_blueprint_event_node", params)

    @mcp.tool()
    def add_blueprint_function_node(
        ctx: Context,
        blueprint_name: str,
        target: str,
        function_name: str,
        params = None,
        node_position = None,
        graph_name: str = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a function call node to a Blueprint's graph.

        Args:
            blueprint_name: Name of the target Blueprint
            target: Target object for the function (component name or self)
            function_name: Name of the function to call
            params: Optional parameters to set on the function node
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name (defaults to EventGraph)
        """
        command_params = {
            "blueprint_name": blueprint_name,
            "target": target,
            "function_name": function_name,
            "params": params or {},
            "node_position": node_position or [0, 0],
            "save": save
        }
        if graph_name:
            command_params["graph_name"] = graph_name
        return _send("add_blueprint_function_node", command_params)

    @mcp.tool()
    def smart_connect_pins(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str,
        auto_convert: bool = True,
        graph_name: str = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Connect two pins in a Blueprint's graph with automatic type conversion.

        Automatically creates conversion nodes if the pin types don't match
        directly (e.g., Float to String).

        Args:
            blueprint_name: Name of the target Blueprint
            source_node_id: ID of the source node
            source_pin: Name of the output pin on the source node
            target_node_id: ID of the target node
            target_pin: Name of the input pin on the target node
            auto_convert: If True, automatically create conversion nodes for type mismatches
            graph_name: Optional target graph name (defaults to EventGraph)
        """
        params = {
            "blueprint_name": blueprint_name,
            "source_node_id": source_node_id,
            "source_pin": source_pin,
            "target_node_id": target_node_id,
            "target_pin": target_pin,
            "auto_convert": auto_convert,
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        return _send("smart_connect_pins", params)

    @mcp.tool()
    def add_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a variable to a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable
            variable_type: Type of the variable (Boolean, Integer, Float, Vector, etc.)
            is_exposed: Whether to expose the variable to the editor
        """
        return _send("add_blueprint_variable", {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
            "variable_type": variable_type,
            "is_exposed": is_exposed,
            "save": save
        })

    @mcp.tool()
    def remove_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Remove a variable from a Blueprint.

        Args:
            blueprint_name: Name or path of the target Blueprint
            variable_name: Name of the variable to remove
            save: Whether to save the Blueprint after removal
        """
        return _send("remove_blueprint_variable", {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
            "save": save
        })

    @mcp.tool()
    def find_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type = None,
        event_type = None,
        graph_name: str = None
    ) -> Dict[str, Any]:
        """
        Find nodes in a Blueprint's graph.

        Args:
            blueprint_name: Name of the target Blueprint
            node_type: Optional type of node to find (Event, Function, Variable, etc.)
            event_type: Optional specific event type to find (BeginPlay, Tick, etc.)
            graph_name: Optional target graph name (defaults to EventGraph)
        """
        params = {
            "blueprint_name": blueprint_name,
            "node_type": node_type,
            "event_type": event_type
        }
        if graph_name:
            params["graph_name"] = graph_name
        return _send("find_blueprint_nodes", params)

    @mcp.tool()
    def spawn_k2_node(
        ctx: Context,
        blueprint_name: str,
        node_type: str,
        node_position = None,
        graph_name: str = None,
        function_name: str = "",
        variable_name: str = "",
        pin_defaults: dict = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Spawn a K2 (Blueprint) node in a Blueprint's graph.

        Args:
            blueprint_name: Name of the target Blueprint
            node_type: Type of K2 node to spawn. Supported types:
                      Control flow: 'IfThenElse'/'Branch', 'Sequence', 'Select',
                                   'SwitchInteger', 'SwitchString'
                      Functions:   'CallFunction' (requires function_name)
                      Variables:   'VariableGet'/'GetVariable', 'VariableSet'/'SetVariable'
                                   (requires variable_name)
                      Comparisons: 'Less', 'LessEqual', 'Greater', 'GreaterEqual',
                                   'Equal', 'NotEqual'
                      Boolean:     'AND', 'OR', 'NOT'
                      Generic:     Any K2Node_ class name (e.g. 'K2Node_EvaluateChooser2')
            node_position: Optional [X, Y] position in the graph
            graph_name: Optional target graph name (defaults to EventGraph)
            function_name: For CallFunction — function to call (e.g. 'ClassName::FuncName')
            variable_name: For VariableGet/VariableSet — name of the Blueprint variable
            pin_defaults: Dict of {pin_name: value} to set default values on pins
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "node_type": node_type,
            "node_position": node_position or [0, 0],
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        if function_name:
            params["function_name"] = function_name
        if variable_name:
            params["variable_name"] = variable_name
        if pin_defaults:
            params["pin_defaults"] = pin_defaults
        return _send("spawn_k2_node", params)

    @mcp.tool()
    def read_blueprint_graph(
        ctx: Context,
        blueprint_name: str,
        graph_name: str = None
    ) -> Dict[str, Any]:
        """
        Read a Blueprint's full structure — class settings, variables, and all graph nodes/pins/connections.

        Args:
            blueprint_name: Name of the Blueprint (or full content path like /Game/AbilitySystem/BPFL_GMAS)
            graph_name: Optional — filter to a specific graph (e.g. "EventGraph" or a function name)
        """
        params = {"blueprint_name": blueprint_name}
        if graph_name:
            params["graph_name"] = graph_name
        return _send("read_blueprint_graph", params)

    @mcp.tool()
    def create_blueprint_function(
        ctx: Context,
        blueprint_name: str,
        function_name: str,
        inputs: list = None,
        outputs: list = None,
        is_pure: bool = False,
        access: str = "Public",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new function graph in a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            function_name: Name of the new function
            inputs: Optional list of input params [{"name": "Speed", "type": "Float"}, ...]
            outputs: Optional list of output params [{"name": "Result", "type": "Boolean"}, ...]
            is_pure: If true, function has no exec pins (pure function)
            access: "Public", "Protected", or "Private"
        """
        params = {
            "blueprint_name": blueprint_name,
            "function_name": function_name,
            "is_pure": is_pure,
            "access": access,
            "save": save
        }
        if inputs:
            params["inputs"] = inputs
        if outputs:
            params["outputs"] = outputs
        return _send("create_blueprint_function", params)

    logger.info("Blueprint node tools registered successfully")
