"""
Hot-reload support for unreal-mcp tool modules.

Watches the tools/ directory for .py file changes and re-registers
modified tools without restarting the server.
"""

import importlib
import logging
import os
import sys
from typing import Dict, List, Tuple

from mcp.server.fastmcp import FastMCP

logger = logging.getLogger("UnrealMCP")

# Registry: filename -> (module_name, register_func_name, [tool_names])
_module_registry: Dict[str, Tuple[str, str, List[str]]] = {}


def track_registration(mcp: FastMCP, module, register_func_name: str) -> List[str]:
    """
    Call a module's register function while tracking which tools it adds.
    Returns the list of tool names that were registered.
    """
    before = set(mcp._tool_manager._tools.keys())
    register_fn = getattr(module, register_func_name)
    register_fn(mcp)
    after = set(mcp._tool_manager._tools.keys())
    new_tools = sorted(after - before)

    filename = os.path.basename(module.__file__)
    _module_registry[filename] = (module.__name__, register_func_name, new_tools)
    logger.debug(f"[hot-reload] Tracked {filename}: {len(new_tools)} tools")
    return new_tools


def reload_module(mcp: FastMCP, filepath: str) -> bool:
    """
    Reload a single tool module and re-register its tools.
    Returns True on success, False on failure (old tools preserved).
    """
    filename = os.path.basename(filepath)

    if filename not in _module_registry:
        logger.warning(f"[hot-reload] Unknown module: {filename}")
        return False

    module_name, register_func_name, old_tool_names = _module_registry[filename]

    # Backup old Tool objects in case reload fails
    old_tools = {}
    for name in old_tool_names:
        tool = mcp._tool_manager._tools.get(name)
        if tool:
            old_tools[name] = tool

    try:
        # Step 1: Remove old tool entries (add_tool() won't overwrite duplicates)
        for name in old_tool_names:
            mcp._tool_manager._tools.pop(name, None)

        # Step 2: Reload the module
        module = sys.modules.get(module_name)
        if module is None:
            logger.error(f"[hot-reload] Module {module_name} not in sys.modules")
            mcp._tool_manager._tools.update(old_tools)
            return False

        module = importlib.reload(module)

        # Step 3: Re-register tools
        before = set(mcp._tool_manager._tools.keys())
        register_fn = getattr(module, register_func_name)
        register_fn(mcp)
        after = set(mcp._tool_manager._tools.keys())
        new_tools = sorted(after - before)

        # Step 4: Update registry
        _module_registry[filename] = (module_name, register_func_name, new_tools)

        logger.info(
            f"[hot-reload] Reloaded {filename}: "
            f"{len(old_tool_names)} old -> {len(new_tools)} new tools"
        )
        if set(new_tools) != set(old_tool_names):
            added = set(new_tools) - set(old_tool_names)
            removed = set(old_tool_names) - set(new_tools)
            if added:
                logger.info(f"[hot-reload]   Added: {added}")
            if removed:
                logger.info(f"[hot-reload]   Removed: {removed}")

        return True

    except Exception as e:
        logger.error(f"[hot-reload] Failed to reload {filename}: {e}", exc_info=True)
        # Restore old tools so the server keeps working
        mcp._tool_manager._tools.update(old_tools)
        _module_registry[filename] = (module_name, register_func_name, old_tool_names)
        return False


async def start_watcher(mcp: FastMCP, tools_dir: str) -> None:
    """
    Watch the tools directory for .py file changes and reload modules.
    Runs as a long-lived async task. Falls back gracefully if watchfiles
    is not installed.
    """
    try:
        from watchfiles import awatch, Change
    except ImportError:
        logger.info(
            "[hot-reload] watchfiles not installed — "
            "tool auto-reload disabled. Install with: pip install watchfiles"
        )
        return

    logger.info(f"[hot-reload] Watching {tools_dir} for changes")

    try:
        async for changes in awatch(tools_dir):
            reloaded = set()
            for change_type, filepath in changes:
                if not filepath.endswith('.py'):
                    continue
                if '__pycache__' in filepath:
                    continue
                basename = os.path.basename(filepath)
                if basename.startswith('__') or basename == 'hot_reload.py':
                    continue
                if basename in reloaded:
                    continue

                if change_type in (Change.modified, Change.added):
                    if basename in _module_registry:
                        logger.info(f"[hot-reload] Detected change: {basename}")
                        reload_module(mcp, filepath)
                        reloaded.add(basename)
                    elif change_type == Change.added:
                        logger.info(
                            f"[hot-reload] New file: {basename} "
                            f"(add to _TOOL_MODULES in unreal_mcp_server.py to enable)"
                        )
    except Exception as e:
        if "Cancelled" not in str(type(e).__name__):
            logger.error(f"[hot-reload] Watcher stopped: {e}", exc_info=True)
