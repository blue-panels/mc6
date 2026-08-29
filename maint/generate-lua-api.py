#!/usr/bin/env python3
"""Generate the public Lua API reference from mc-lua.c annotations."""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


TAG_RE = re.compile(
    r"\s+@(workspace|capability|mutation|errors|summary)\s+([^@]+?)(?=\s+@|$)"
)


def parse_entry(text: str) -> dict[str, object]:
    tags = {name: value.strip() for name, value in TAG_RE.findall(text)}
    signature = TAG_RE.split(text, maxsplit=1)[0].strip()
    return {
        "signature": signature,
        "workspace": tags.get("workspace", "any"),
        "capability": tags.get("capability", "—"),
        "mutation": tags.get("mutation", "no"),
        "errors": [item.strip() for item in tags.get("errors", "").split(",") if item.strip()],
        "summary": tags.get("summary", ""),
    }


def annotations(source: Path) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    methods: list[dict[str, object]] = []
    callbacks: list[dict[str, object]] = []
    text = source.read_text(encoding="utf-8")

    for comment_match in re.finditer(r"/\*\*(.*?)\*/", text, re.DOTALL):
        body = re.sub(r"\n\s*\*\s?", " ", comment_match.group(1)).strip()
        entries = re.findall(
            r"@(lua-callback|lua)\s+(.*?)(?=\s+@lua(?:-callback)?\s+|$)", body
        )
        owner: dict[str, object] | None = None
        for kind, value in entries:
            entry = parse_entry(value.strip())
            if kind == "lua":
                owner = entry
                methods.append(entry)
                continue
            if owner is not None:
                for key in ("workspace", "capability"):
                    if entry[key] in ("any", "—"):
                        entry[key] = owner[key]
            callbacks.append(entry)

    return methods, callbacks


def validate_coverage(source: Path, methods: list[dict[str, object]], callbacks: list[dict[str, object]]) -> None:
    text = source.read_text(encoding="utf-8")

    annotated_functions: dict[str, int] = {}
    for match in re.finditer(
        r"/\*\*(?P<doc>.*?)\*/\s*static\s+int\s+(?P<function>mc_lua_[a-z0-9_]+)\s*\(",
        text,
        re.DOTALL,
    ):
        count = len(re.findall(r"@lua\s+", match.group("doc")))
        if count:
            annotated_functions[match.group("function")] = count

    registered_functions = {
        match.group("function")
        for match in re.finditer(
            r"lua_pushcfunction\s*\(lua,\s*(?P<function>mc_lua_[a-z0-9_]+)\s*\);"
            r"\s*lua_setfield\s*\(lua,\s*-2,\s*\"(?P<field>[^\"]+)\"\s*\)",
            text,
        )
        if not match.group("field").startswith("__")
    }

    # methods of userdata objects are handed out by an __index dispatcher:
    # mc_lua_handle_index() for panels, editors and viewers, mc_lua_screen_index() for screens
    index_functions = list(
        re.finditer(
            r"static\s+int\s+mc_lua_[a-z0-9_]*index\s*\(lua_State \*lua\)\s*\{.*?^}\n",
            text,
            re.DOTALL | re.MULTILINE,
        )
    )
    if not any("mc_lua_handle_index" in match.group(0) for match in index_functions):
        raise SystemExit("cannot locate mc_lua_handle_index()")
    for index_function in index_functions:
        registered_functions.update(
            re.findall(
                r"lua_pushcfunction\s*\(lua,\s*(mc_lua_[a-z0-9_]+)\s*\)", index_function.group(0)
            )
        )

    levels_match = re.search(r"const char \*const levels\[\]\s*=\s*\{(?P<levels>.*?)\};", text)
    if levels_match is None:
        raise SystemExit("cannot locate mc.log levels")
    log_levels = set(re.findall(r'\"([^\"]+)\"', levels_match.group("levels")))
    annotated_log_levels = {
        match.group(1)
        for method in methods
        if (match := re.match(r"mc\.log\.([a-z0-9_]+)\(", str(method["signature"]))) is not None
    }
    if log_levels != annotated_log_levels:
        raise SystemExit(
            "mc.log annotation mismatch: "
            f"missing={sorted(log_levels - annotated_log_levels)}, "
            f"extra={sorted(annotated_log_levels - log_levels)}"
        )

    registered_functions.add("mc_lua_log_message")
    missing = sorted(registered_functions - set(annotated_functions))
    stale = sorted(set(annotated_functions) - registered_functions)
    if missing or stale:
        raise SystemExit(f"Lua function annotation mismatch: missing={missing}, stale={stale}")
    if sum(annotated_functions.values()) != len(methods):
        raise SystemExit("some @lua annotations are not attached to a registered C entrypoint")

    registered_callbacks = set(
        re.findall(r'mc_lua_panel_callback_ref\s*\(lua,\s*1,\s*\"([^\"]+)\"', text)
    )
    file_handler_register = re.search(
        r"static\s+int\s+mc_lua_file_handler_register\s*\(lua_State \*lua\)\s*\{.*?^}\n",
        text,
        re.DOTALL | re.MULTILINE,
    )
    if file_handler_register is not None and re.search(
        r'lua_getfield\s*\(lua,\s*1,\s*\"handler\"\s*\)', file_handler_register.group(0)
    ):
        registered_callbacks.add("handler")
    annotated_callbacks = {
        match.group(1)
        for callback in callbacks
        if (match := re.match(r"([a-z0-9_]+)\(", str(callback["signature"]))) is not None
    }
    missing_callbacks = sorted(registered_callbacks - annotated_callbacks)
    if missing_callbacks:
        raise SystemExit(f"Lua callback annotations missing: {missing_callbacks}")


def markdown_escape(value: object) -> str:
    return str(value).replace("|", "\\|")


def write_markdown(path: Path, methods: list[dict[str, object]], callbacks: list[dict[str, object]]) -> None:
    grouped: dict[str, list[dict[str, object]]] = defaultdict(list)
    for method in methods:
        grouped[str(method["workspace"])].append(method)

    lines = [
        "# Lua API reference",
        "",
        "This file is generated from annotations in `src/lua/mc-lua.c`.",
        "Do not edit it manually; run `python3 maint/generate-lua-api.py`.",
        "",
    ]
    order = [name for name in ("mc", "mcedit", "mcview", "mcterm", "mcdiff", "any") if name in grouped]
    order.extend(sorted(set(grouped) - set(order)))
    for workspace in order:
        lines.extend(
            [
                f"## Workspace `{workspace}`",
                "",
                "| Lua method | Description | Capability | Mutation |",
                "|---|---|---|---|",
            ]
        )
        for method in sorted(grouped[workspace], key=lambda item: str(item["signature"])):
            lines.append(
                f"| `{markdown_escape(method['signature'])}` | {markdown_escape(method['summary'])} | "
                f"`{markdown_escape(method['capability'])}` | {method['mutation']} |"
            )
        lines.append("")

    lines.extend(["## Callback contracts", "", "| Callback | Workspace | Capability |", "|---|---|---|"])
    for callback in sorted(callbacks, key=lambda item: str(item["signature"])):
        lines.append(
            f"| `{markdown_escape(callback['signature'])}` | `{callback['workspace']}` | "
            f"`{callback['capability']}` |"
        )
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=root / "src/lua/mc-lua.c")
    parser.add_argument("--markdown", type=Path, default=root / "doc/LUA_API_REFERENCE.md")
    parser.add_argument("--json", type=Path, default=root / "doc/lua-api.json")
    args = parser.parse_args()

    methods, callbacks = annotations(args.source)
    if not methods:
        raise SystemExit("no @lua annotations found")
    undocumented = [str(method["signature"]) for method in methods if not method["summary"]]
    if undocumented:
        raise SystemExit(f"Lua methods without @summary: {', '.join(undocumented)}")

    for kind, entries in (("method", methods), ("callback", callbacks)):
        signatures = [str(entry["signature"]) for entry in entries]
        duplicates = sorted({signature for signature in signatures if signatures.count(signature) > 1})
        if duplicates:
            raise SystemExit(f"duplicate Lua {kind} annotations: {', '.join(duplicates)}")

    validate_coverage(args.source, methods, callbacks)

    payload = {"schema_version": 1, "methods": methods, "callbacks": callbacks}
    write_markdown(args.markdown, methods, callbacks)
    args.json.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
