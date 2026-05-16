"""
Animation, AnimGraph, and Level Sequence Tools for Unreal MCP.

This module provides tools for creating blend spaces, anim blueprints,
adding anim notifies, playing animations, managing level sequences,
and full AnimGraph node manipulation (Tiers 1-4).
"""

import logging
from typing import Dict, Any, Optional, List
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

def register_animation_tools(mcp: FastMCP):
    """Register animation, AnimGraph, and sequence tools with the MCP server."""

    @mcp.tool()
    def create_blend_space(
        ctx: Context,
        name: str,
        skeleton_path: str,
        path: str = "/Game/Animations",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new BlendSpace asset.

        Args:
            name: Name of the BlendSpace to create
            skeleton_path: Full content path to the skeleton (e.g., "/Game/Characters/Mannequin/Mesh/SK_Mannequin_Skeleton")
            path: Content browser path where the BlendSpace should be created
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("create_blend_space", {
            "name": name,
            "skeleton_path": skeleton_path,
            "path": path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def add_anim_notify(
        ctx: Context,
        animation_path: str,
        notify_name: str,
        time: float,
        notify_class: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add an AnimNotify to an animation sequence at a specific time.

        Args:
            animation_path: Full content path to the animation (e.g., "/Game/Animations/AM_Attack")
            notify_name: Name for the notify event
            time: Time in seconds where the notify should trigger
            notify_class: Optional full class path for a custom notify class (defaults to UAnimNotify)
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "animation_path": animation_path,
            "notify_name": notify_name,
            "time": time,
            "save": save
        }
        if notify_class:
            params["notify_class"] = notify_class

        result = conn.send_command("add_anim_notify", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def add_anim_notify_state(
        ctx: Context,
        animation_path: str,
        notify_name: str,
        start_time: float,
        end_time: float = -1.0,
        duration: float = -1.0,
        notify_class: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a duration-based AnimNotifyState to an animation (montage or sequence).

        Unlike add_anim_notify (point-in-time), this creates a notify with a start
        and end time — used for melee trace windows, particle effects, sound loops, etc.

        Must provide either end_time or duration (not both).

        Args:
            animation_path: Full content path to the animation (e.g., "/Game/Animations/AM_Attack")
            notify_name: Name for the notify state event
            start_time: Start time in seconds where the notify state begins
            end_time: End time in seconds where the notify state ends (alternative to duration)
            duration: Duration in seconds (alternative to end_time)
            notify_class: Optional full class path for a custom UAnimNotifyState subclass
                         (e.g., "/Script/MyProject.AnimNotifyState_MeleeTrace")
            save: Whether to save the asset to disk (default True)
        """
        params = {
            "animation_path": animation_path,
            "notify_name": notify_name,
            "start_time": start_time,
            "save": save
        }
        if end_time >= 0:
            params["end_time"] = end_time
        elif duration >= 0:
            params["duration"] = duration
        else:
            return {"error": "Must provide either 'end_time' or 'duration'"}
        if notify_class:
            params["notify_class"] = notify_class
        return _send("add_anim_notify_state", params)

    @mcp.tool()
    def add_skeletal_mesh_socket(
        ctx: Context,
        socket_name: str,
        bone_name: str,
        skeleton_path: str = "",
        skeletal_mesh_path: str = "",
        relative_location: Optional[List[float]] = None,
        relative_rotation: Optional[List[float]] = None,
        relative_scale: Optional[List[float]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a socket to a skeleton, attached to a specific bone.

        Provide either skeleton_path (direct) or skeletal_mesh_path (resolves to skeleton).

        Args:
            socket_name: Name for the new socket (e.g., "MeleeTraceStart", "VFX_Attach")
            bone_name: Parent bone name (e.g., "hand_r", "spine_03")
            skeleton_path: Full content path to the USkeleton asset
            skeletal_mesh_path: Full content path to a USkeletalMesh (resolves to its skeleton)
            relative_location: Optional [X, Y, Z] offset from bone
            relative_rotation: Optional [Pitch, Yaw, Roll] rotation offset
            relative_scale: Optional [X, Y, Z] scale (defaults to [1,1,1])
            save: Whether to save the skeleton to disk (default True)
        """
        if not skeleton_path and not skeletal_mesh_path:
            return {"error": "Must provide either 'skeleton_path' or 'skeletal_mesh_path'"}

        params: Dict[str, Any] = {
            "socket_name": socket_name,
            "bone_name": bone_name,
            "save": save
        }
        if skeleton_path:
            params["skeleton_path"] = skeleton_path
        else:
            params["skeletal_mesh_path"] = skeletal_mesh_path
        if relative_location is not None:
            params["relative_location"] = relative_location
        if relative_rotation is not None:
            params["relative_rotation"] = relative_rotation
        if relative_scale is not None:
            params["relative_scale"] = relative_scale
        return _send("add_skeletal_mesh_socket", params)

    @mcp.tool()
    def play_animation(
        ctx: Context,
        actor_name: str,
        animation_path: str,
        loop: bool = False
    ) -> Dict[str, Any]:
        """
        Play an animation on an actor's skeletal mesh component.

        Args:
            actor_name: Name or label of the actor in the level
            animation_path: Full content path to the animation asset
            loop: Whether to loop the animation (default False)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("play_animation", {
            "actor_name": actor_name,
            "animation_path": animation_path,
            "loop": loop
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def create_anim_blueprint(
        ctx: Context,
        name: str,
        skeleton_path: str,
        path: str = "/Game/Animations",
        parent_class: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new Animation Blueprint asset.

        Args:
            name: Name of the AnimBlueprint to create
            skeleton_path: Full content path to the skeleton
            path: Content browser path where the AnimBlueprint should be created
            parent_class: Optional full class path for parent class (defaults to UAnimInstance)
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "name": name,
            "skeleton_path": skeleton_path,
            "path": path,
            "save": save
        }
        if parent_class:
            params["parent_class"] = parent_class

        result = conn.send_command("create_anim_blueprint", params)
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def create_sequence(
        ctx: Context,
        name: str,
        path: str = "/Game/Sequences",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a new Level Sequence asset.

        Args:
            name: Name of the LevelSequence to create
            path: Content browser path where the sequence should be created
            save: Whether to save the asset to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("create_sequence", {
            "name": name,
            "path": path,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def add_actor_to_sequence(
        ctx: Context,
        sequence_path: str,
        actor_name: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add an actor from the level to a Level Sequence as a possessable binding.

        Args:
            sequence_path: Full content path to the LevelSequence
            actor_name: Name or label of the actor in the level
            save: Whether to save the sequence to disk (default True)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        result = conn.send_command("add_actor_to_sequence", {
            "sequence_path": sequence_path,
            "actor_name": actor_name,
            "save": save
        })
        return result or {"error": "No response from Unreal Engine"}

    @mcp.tool()
    def play_sequence(
        ctx: Context,
        sequence_path: str,
        start_time: float = 0.0,
        loop: bool = False
    ) -> Dict[str, Any]:
        """
        Play a Level Sequence in the editor world.

        Args:
            sequence_path: Full content path to the LevelSequence
            start_time: Time in seconds to start playback from (default 0.0)
            loop: Whether to loop the sequence (default False)
        """
        from unreal_mcp_server import get_unreal_connection

        conn = get_unreal_connection()
        if not conn:
            return {"error": "Not connected to Unreal Engine"}

        params = {
            "sequence_path": sequence_path,
            "loop": loop
        }
        if start_time > 0.0:
            params["start_time"] = start_time

        result = conn.send_command("play_sequence", params)
        return result or {"error": "No response from Unreal Engine"}

    # ========================================================================
    # Tier 1 — Core AnimGraph Node Operations
    # ========================================================================

    @mcp.tool()
    def read_anim_graph(
        ctx: Context,
        blueprint_path: str,
        graph_name: str = ""
    ) -> Dict[str, Any]:
        """
        Read an Animation Blueprint's AnimGraph structure — all nodes, pins, and connections.

        Args:
            blueprint_path: Full content path to the AnimBlueprint (e.g., "/Game/Animations/Blueprints/Humanoid/ABP_MotionMatching")
            graph_name: Optional — filter to a specific graph (e.g. "AnimGraph")
        """
        params = {"blueprint_path": blueprint_path}
        if graph_name:
            params["graph_name"] = graph_name
        return _send("read_anim_graph", params)

    @mcp.tool()
    def add_anim_graph_node(
        ctx: Context,
        blueprint_path: str,
        node_class: str,
        graph_name: str = "",
        position: Optional[Dict[str, int]] = None,
        properties: Optional[Dict[str, Any]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add an AnimGraph node to an Animation Blueprint.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node_class: Class name of the anim node (e.g. "AnimGraphNode_Slot", "Slot", "MotionMatching", "PoseSearchHistoryCollector")
            graph_name: Optional target graph name (defaults to root AnimGraph)
            position: Optional {"x": int, "y": int} position in the graph
            properties: Optional dict of initial property values to set on the node
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {"blueprint_path": blueprint_path, "node_class": node_class, "save": save}
        if graph_name:
            params["graph_name"] = graph_name
        if position:
            params["position"] = position
        if properties:
            params["properties"] = properties
        return _send("add_anim_graph_node", params)

    @mcp.tool()
    def connect_anim_pins(
        ctx: Context,
        blueprint_path: str,
        source_node: str,
        source_pin: str,
        target_node: str,
        target_pin: str,
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Connect two pins in an AnimGraph (pose links or data pins).

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            source_node: GUID of the source node
            source_pin: Name of the output pin on the source node
            target_node: GUID of the target node
            target_pin: Name of the input pin on the target node
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "source_node": source_node,
            "source_pin": source_pin,
            "target_node": target_node,
            "target_pin": target_pin,
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        return _send("connect_anim_pins", params)

    @mcp.tool()
    def set_anim_node_property(
        ctx: Context,
        blueprint_path: str,
        node: str,
        property_name: str,
        value: Any,
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a property on an AnimGraph node's inner FAnimNode struct.

        Property paths like "SlotName" auto-prefix to "Node.SlotName". Use dot-paths
        for nested properties (e.g. "Node.Database").

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the target node (or node_index)
            property_name: Property name or dot-path (e.g. "SlotName", "Node.BlendTime")
            value: The value to set (string, number, bool, etc.)
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "node": node,
            "property_name": property_name,
            "value": value,
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        return _send("set_anim_node_property", params)

    @mcp.tool()
    def delete_anim_graph_node(
        ctx: Context,
        blueprint_path: str,
        node: str,
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Delete an AnimGraph node and break all its pin connections.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the node to delete
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {"blueprint_path": blueprint_path, "node": node, "save": save}
        if graph_name:
            params["graph_name"] = graph_name
        return _send("delete_anim_graph_node", params)

    @mcp.tool()
    def disconnect_anim_pin(
        ctx: Context,
        blueprint_path: str,
        node: str,
        pin_name: str,
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Disconnect all links from a specific pin on an AnimGraph node.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the target node
            pin_name: Name of the pin to disconnect (accepts input or output)
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "node": node,
            "pin_name": pin_name,
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        return _send("disconnect_anim_pin", params)

    @mcp.tool()
    def find_anim_graph_nodes(
        ctx: Context,
        blueprint_path: str,
        node_class: str = "",
        property_filter: Optional[Dict[str, Any]] = None,
        graph_name: str = ""
    ) -> Dict[str, Any]:
        """
        Search for AnimGraph nodes by class type or property values.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node_class: Optional class name filter (e.g. "AnimGraphNode_Slot")
            property_filter: Optional dict of property name/value pairs to match
            graph_name: Optional target graph name
        """
        params: Dict[str, Any] = {"blueprint_path": blueprint_path}
        if node_class:
            params["node_class"] = node_class
        if property_filter:
            params["property_filter"] = property_filter
        if graph_name:
            params["graph_name"] = graph_name
        return _send("find_anim_graph_nodes", params)

    # ========================================================================
    # Tier 2 — State Machine Operations
    # ========================================================================

    @mcp.tool()
    def add_state_machine(
        ctx: Context,
        blueprint_path: str,
        name: str = "StateMachine",
        graph_name: str = "",
        position: Optional[Dict[str, int]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a state machine node to an AnimGraph.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            name: Name for the state machine
            graph_name: Optional target graph name
            position: Optional {"x": int, "y": int} position
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {"blueprint_path": blueprint_path, "name": name, "save": save}
        if graph_name:
            params["graph_name"] = graph_name
        if position:
            params["position"] = position
        return _send("add_state_machine", params)

    @mcp.tool()
    def add_state(
        ctx: Context,
        blueprint_path: str,
        state_machine: str,
        name: str = "NewState",
        type: str = "State",
        position: Optional[Dict[str, int]] = None,
        alias_target: str = "",
        global_alias: bool = False,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a state to a state machine.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            state_machine: GUID of the state machine node
            name: Name for the new state
            type: Node type — "State" (default), "Conduit" (pass-through condition),
                  or "Alias" (references another state)
            position: Optional {"x": int, "y": int} position
            alias_target: For Alias type — GUID of the state to alias
            global_alias: For Alias type — if true, alias is global (matches any state)
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "state_machine": state_machine,
            "name": name,
            "save": save
        }
        if type != "State":
            params["type"] = type
        if position:
            params["position"] = position
        if alias_target:
            params["alias_target"] = alias_target
        if global_alias:
            params["global_alias"] = global_alias
        return _send("add_state", params)

    @mcp.tool()
    def add_state_transition(
        ctx: Context,
        blueprint_path: str,
        state_machine: str,
        from_state: str,
        to_state: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a transition between two states in a state machine.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            state_machine: GUID of the state machine node
            from_state: GUID of the source state
            to_state: GUID of the destination state
            save: Whether to save after modification
        """
        return _send("add_state_transition", {
            "blueprint_path": blueprint_path,
            "state_machine": state_machine,
            "from_state": from_state,
            "to_state": to_state,
            "save": save
        })

    @mcp.tool()
    def rename_state(
        ctx: Context,
        blueprint_path: str,
        state_machine: str,
        state: str,
        name: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Rename a state or conduit in a state machine.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            state_machine: GUID of the state machine node
            state: GUID of the state to rename
            name: New name for the state
            save: Whether to save after modification
        """
        return _send("rename_state", {
            "blueprint_path": blueprint_path,
            "state_machine": state_machine,
            "state": state,
            "name": name,
            "save": save
        })

    @mcp.tool()
    def set_state_animation(
        ctx: Context,
        blueprint_path: str,
        state: str,
        node_class: str,
        position: Optional[Dict[str, int]] = None,
        properties: Optional[Dict[str, Any]] = None,
        auto_connect: bool = True,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add an animation node inside a state's inner graph and optionally auto-connect to result.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            state: GUID of the state node
            node_class: Class name of the anim node to place in the state (e.g. "SequencePlayer", "MotionMatching")
            position: Optional {"x": int, "y": int} position
            properties: Optional dict of initial property values
            auto_connect: Whether to auto-connect the node's output to the state result (default True)
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "state": state,
            "node_class": node_class,
            "auto_connect": auto_connect,
            "save": save
        }
        if position:
            params["position"] = position
        if properties:
            params["properties"] = properties
        return _send("set_state_animation", params)

    @mcp.tool()
    def read_state_machine(
        ctx: Context,
        blueprint_path: str,
        state_machine: str,
        include_inner_nodes: bool = False
    ) -> Dict[str, Any]:
        """
        Read a state machine's structure — states, transitions, and connections.

        Returns all states (with type: State/Conduit/Alias), transitions (with
        crossfade/priority/bidirectional properties), and entry state info.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            state_machine: GUID of the state machine node
            include_inner_nodes: If true, serialize the full node graph inside each
                                state and transition rule graph (nodes, pins, connections)
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "state_machine": state_machine
        }
        if include_inner_nodes:
            params["include_inner_nodes"] = True
        return _send("read_state_machine", params)

    # ========================================================================
    # Tier 3 — Advanced AnimGraph Operations
    # ========================================================================

    @mcp.tool()
    def add_anim_layer(
        ctx: Context,
        blueprint_path: str,
        layer_type: str = "LinkedAnimGraph",
        layer_name: str = "",
        graph_name: str = "",
        position: Optional[Dict[str, int]] = None,
        properties: Optional[Dict[str, Any]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a LinkedAnimGraph or LinkedAnimLayer node to the AnimGraph.

        For LinkedAnimLayer, providing layer_name will auto-create the backing
        AnimGraph function (if it doesn't exist) and wire the node to it.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            layer_type: "LinkedAnimGraph" or "LinkedAnimLayer"
            layer_name: For LinkedAnimLayer — name of the layer function to create/link
            graph_name: Optional target graph name
            position: Optional {"x": int, "y": int} position
            properties: Optional dict of initial property values
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "layer_type": layer_type,
            "save": save
        }
        if layer_name:
            params["layer_name"] = layer_name
        if graph_name:
            params["graph_name"] = graph_name
        if position:
            params["position"] = position
        if properties:
            params["properties"] = properties
        return _send("add_anim_layer", params)

    @mcp.tool()
    def add_blend_node(
        ctx: Context,
        blueprint_path: str,
        blend_type: str,
        graph_name: str = "",
        position: Optional[Dict[str, int]] = None,
        properties: Optional[Dict[str, Any]] = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a blend node to the AnimGraph.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            blend_type: One of: "LayeredBlendPerBone", "BlendPosesByBool", "BlendPosesByInt", "TwoWayBlend", "MultiWayBlend"
            graph_name: Optional target graph name
            position: Optional {"x": int, "y": int} position
            properties: Optional dict of initial property values
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "blend_type": blend_type,
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        if position:
            params["position"] = position
        if properties:
            params["properties"] = properties
        return _send("add_blend_node", params)

    @mcp.tool()
    def add_blend_pose_pin(
        ctx: Context,
        blueprint_path: str,
        node: str,
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Add a dynamic blend pose pin to a blend node (LayeredBlendPerBone, BlendListByInt, etc.).

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the blend node
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {"blueprint_path": blueprint_path, "node": node, "save": save}
        if graph_name:
            params["graph_name"] = graph_name
        return _send("add_blend_pose_pin", params)

    @mcp.tool()
    def set_anim_blueprint_parent(
        ctx: Context,
        blueprint_path: str,
        parent_class: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Reparent an Animation Blueprint to a different AnimInstance class.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            parent_class: Full class path of the new parent (e.g. "/Script/MyProject.MyAnimInstance")
            save: Whether to save after modification
        """
        return _send("set_anim_blueprint_parent", {
            "blueprint_path": blueprint_path,
            "parent_class": parent_class,
            "save": save
        })

    @mcp.tool()
    def compile_anim_blueprint(
        ctx: Context,
        blueprint_path: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Compile an Animation Blueprint and return the compilation status.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            save: Whether to save after successful compilation
        """
        return _send("compile_anim_blueprint", {
            "blueprint_path": blueprint_path,
            "save": save
        })

    # ========================================================================
    # Tier 4 — PoseSearch / Motion Matching Operations
    # ========================================================================

    @mcp.tool()
    def configure_motion_matching(
        ctx: Context,
        blueprint_path: str,
        node: str,
        database: str = "",
        blend_time: Optional[float] = None,
        interrupt_mode: str = "",
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Configure a MotionMatching AnimGraph node's properties.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the MotionMatching node
            database: Optional PoseSearch database asset path
            blend_time: Optional blend duration in seconds
            interrupt_mode: Optional interrupt behavior mode
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {"blueprint_path": blueprint_path, "node": node, "save": save}
        if database:
            params["database"] = database
        if blend_time is not None:
            params["blend_time"] = blend_time
        if interrupt_mode:
            params["interrupt_mode"] = interrupt_mode
        if graph_name:
            params["graph_name"] = graph_name
        return _send("configure_motion_matching", params)

    @mcp.tool()
    def configure_history_collector(
        ctx: Context,
        blueprint_path: str,
        node: str,
        pose_count: Optional[int] = None,
        sample_interval: Optional[float] = None,
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Configure a PoseSearchHistoryCollector AnimGraph node's properties.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the PoseSearchHistoryCollector node
            pose_count: Optional number of poses to collect
            sample_interval: Optional sample timing interval
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {"blueprint_path": blueprint_path, "node": node, "save": save}
        if pose_count is not None:
            params["pose_count"] = pose_count
        if sample_interval is not None:
            params["sample_interval"] = sample_interval
        if graph_name:
            params["graph_name"] = graph_name
        return _send("configure_history_collector", params)

    # ========================================================================
    # State Machine Utilities
    # ========================================================================

    @mcp.tool()
    def set_state_entry(
        ctx: Context,
        blueprint_path: str,
        state_machine: str,
        state: str,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set the entry/default state of a state machine.

        Changes which state the state machine enters by default (the one connected
        to the Entry node).

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            state_machine: GUID of the state machine node
            state: GUID of the state to set as entry
            save: Whether to save after modification
        """
        return _send("set_state_entry", {
            "blueprint_path": blueprint_path,
            "state_machine": state_machine,
            "state": state,
            "save": save
        })

    @mcp.tool()
    def bind_transition_condition(
        ctx: Context,
        blueprint_path: str,
        state_machine: str,
        transition: str,
        crossfade_duration: float | None = None,
        priority: int | None = None,
        blend_mode: str = "",
        logic_type: str = "",
        automatic_rule: bool | None = None,
        automatic_rule_trigger_time: float | None = None,
        bidirectional: bool | None = None,
        disabled: bool | None = None,
        node_class: str = "",
        auto_connect: bool = True,
        variable_name: str = "",
        function_name: str = "",
        pin_defaults: Dict[str, Any] | None = None,
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Configure a state machine transition's properties and optionally add a condition node.

        Sets crossfade duration, blend mode, priority, automatic rules, and other
        transition properties. Can also create a K2 (Blueprint) condition node in the
        transition's rule graph and auto-wire it to the result.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            state_machine: GUID of the state machine node
            transition: GUID of the transition to configure
            crossfade_duration: Blend duration in seconds
            priority: Priority order (lower = higher priority when multiple transitions fire)
            blend_mode: Interpolation type (Linear, Cubic, HermiteCubic, Sinusoidal,
                       QuadraticInOut, CubicInOut, CircularIn, CircularOut, ExpIn, ExpOut, etc.)
            logic_type: Transition type (StandardBlend, Inertialization, Custom)
            automatic_rule: Auto-trigger based on sequence player remaining time
            automatic_rule_trigger_time: When to trigger relative to sequence end (<0 = use crossfade duration)
            bidirectional: Allow transition in both directions
            disabled: Disable this transition from being enterable
            node_class: K2 node type to create as the condition. Supported types:
                       "GetVariable" (requires variable_name), "CallFunction" (requires function_name),
                       "Less", "LessEqual", "Greater", "GreaterEqual", "Equal", "NotEqual" (float comparisons),
                       "AND", "OR", "NOT" (boolean logic), "TimeRemaining" (GetRelevantAnimTimeRemaining)
            auto_connect: If true (default), wire the condition node's output to TransitionResult
            variable_name: For GetVariable — name of the Blueprint variable to read
            function_name: For CallFunction — function to call (e.g. "ClassName::FuncName" or "FuncName")
            pin_defaults: Dict of {pin_name: value} to set default values on created node pins
                         (e.g. {"B": "0.2"} to set threshold on a comparison node)
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "state_machine": state_machine,
            "transition": transition,
            "save": save
        }
        if crossfade_duration is not None:
            params["crossfade_duration"] = crossfade_duration
        if priority is not None:
            params["priority"] = priority
        if blend_mode:
            params["blend_mode"] = blend_mode
        if logic_type:
            params["logic_type"] = logic_type
        if automatic_rule is not None:
            params["automatic_rule"] = automatic_rule
        if automatic_rule_trigger_time is not None:
            params["automatic_rule_trigger_time"] = automatic_rule_trigger_time
        if bidirectional is not None:
            params["bidirectional"] = bidirectional
        if disabled is not None:
            params["disabled"] = disabled
        if node_class:
            params["node_class"] = node_class
            params["auto_connect"] = auto_connect
        if variable_name:
            params["variable_name"] = variable_name
        if function_name:
            params["function_name"] = function_name
        if pin_defaults:
            params["pin_defaults"] = pin_defaults
        return _send("bind_transition_condition", params)

    # ========================================================================
    # K2 Pin Connections (general-purpose)
    # ========================================================================

    @mcp.tool()
    def connect_k2_pins(
        ctx: Context,
        blueprint_path: str,
        source_node: str,
        target_node: str,
        source_pin: str = "",
        target_pin: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Connect K2 (Blueprint) nodes in any graph — transition rules, event graphs, etc.

        Wires an output pin on the source node to an input pin on the target node.
        Both nodes must be in the same graph. If pin names are omitted, the tool
        finds the first compatible output→input pair automatically.

        Works in transition rule graphs (for wiring condition logic), state machine
        inner graphs, and AnimBlueprint event graphs.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            source_node: GUID of the source node (output side)
            target_node: GUID of the target node (input side)
            source_pin: Optional output pin name (default: auto-detect, prefers ReturnValue)
            target_pin: Optional input pin name (default: auto-detect, first non-exec input)
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "source_node": source_node,
            "target_node": target_node,
            "save": save
        }
        if source_pin:
            params["source_pin"] = source_pin
        if target_pin:
            params["target_pin"] = target_pin
        return _send("connect_k2_pins", params)

    # ========================================================================
    # Property Access Binding
    # ========================================================================

    @mcp.tool()
    def bind_anim_pin_to_property(
        ctx: Context,
        blueprint_path: str,
        node: str,
        pin_name: str,
        property_path: str,
        binding_type: str = "Property",
        context: str = "",
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Set a Property Access binding on an AnimGraph node pin.

        Binds a pin to a Blueprint variable or property path so it updates
        automatically each frame (like dragging a variable onto a pin in the editor).

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the target AnimGraph node
            pin_name: Name of the pin to bind (e.g. "Alpha", "BlendWeight", "Speed")
            property_path: Source property path — dot-separated string (e.g. "Speed",
                          "MovementComponent.MaxSpeed") or JSON array for complex paths
            binding_type: "Property" (default) or "Function" for binding to a function return value
            context: Execution context — "Automatic" (default), "ThreadSafe",
                    "GameThreadPre", "GameThreadPost"
            graph_name: Optional target graph name
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "node": node,
            "pin_name": pin_name,
            "property_path": property_path,
            "save": save
        }
        if binding_type and binding_type != "Property":
            params["binding_type"] = binding_type
        if context:
            params["context"] = context
        if graph_name:
            params["graph_name"] = graph_name
        return _send("bind_anim_pin_to_property", params)

    # ========================================================================
    # AnimNode Function Binding
    # ========================================================================

    @mcp.tool()
    def bind_anim_node_function(
        ctx: Context,
        blueprint_path: str,
        node: str,
        event: str,
        function_name: str,
        graph_name: str = "",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Bind an AnimBP function to an AnimGraph node event.

        AnimGraph nodes have lifecycle events (OnInitialUpdate, OnBecomeRelevant,
        OnUpdate) that can call functions in the AnimBP. Motion Matching nodes
        additionally have OnMotionMatchingStateUpdated.

        Create the target function first with create_anim_graph_function, then
        bind it to a node with this tool.

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            node: GUID of the target AnimGraph node
            event: Event to bind. Supported values:
                   "OnInitialUpdate" — called on first update
                   "OnBecomeRelevant" — called when node goes from 0 to any weight
                   "OnUpdate" — called every update tick
                   "OnMotionMatchingStateUpdated" — Motion Matching state change (MM nodes only)
            function_name: Name of the AnimBP function to bind
            graph_name: Optional target graph name (default: root AnimGraph)
            save: Whether to save after modification
        """
        params: Dict[str, Any] = {
            "blueprint_path": blueprint_path,
            "node": node,
            "event": event,
            "function_name": function_name,
            "save": save
        }
        if graph_name:
            params["graph_name"] = graph_name
        return _send("bind_anim_node_function", params)

    @mcp.tool()
    def create_anim_graph_function(
        ctx: Context,
        blueprint_path: str,
        function_name: str,
        type: str = "anim_event",
        save: bool = True
    ) -> Dict[str, Any]:
        """
        Create a function in an Animation Blueprint.

        Supported types:
        - "anim_event" (default): Thread-safe function for AnimNode callbacks
          Signature: void(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
          After creation, bind to a node with bind_anim_node_function.

        - "anim_layer": AnimGraph layer function (UAnimationGraph) for
          LinkedAnimLayer nodes. Creates a pose-in/pose-out graph that can be
          targeted by add_anim_layer(layer_name=...).

        Args:
            blueprint_path: Full content path to the AnimBlueprint
            function_name: Name for the new function
            type: "anim_event" or "anim_layer"
            save: Whether to save after creation
        """
        return _send("create_anim_graph_function", {
            "blueprint_path": blueprint_path,
            "function_name": function_name,
            "type": type,
            "save": save
        })
