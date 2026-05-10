#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI

"""Generate a Markdown CLI reference for one application from a composition spec.

The spec YAML lists root-level CLI params (version flag, config flag, etc.) and the
set of schema YAML files that compose the application.  Subcommands with the same name
across multiple schemas are merged into a single heading.

Usage:
    python3 tools/config_gen/app_doc_schema.py apps/config/gnb.app.yaml -o docs/gnb_config.md
    python3 tools/config_gen/app_doc_schema.py apps/config/du.app.yaml
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import IO, Optional

import yaml

sys.path.insert(0, str(Path(__file__).parent))
from schema import Parameter, Range, Schema, Subcommand, load_schema
from doc_schema import render_schema


# ---------------------------------------------------------------------------
# Merged tree
# ---------------------------------------------------------------------------


@dataclass
class InlineParam:
    """A parameter directly declared in the app spec (not from a schema file)."""

    flag: str
    description: str
    type: Optional[str] = None
    default: Optional[str] = None
    range: Optional[Range] = None


@dataclass
class MergedSubcommand:
    """A subcommand node that may aggregate params from multiple schemas."""

    name: str
    description: str
    schema_params: list[Parameter] = field(default_factory=list)
    subcommands: dict[str, "MergedSubcommand"] = field(default_factory=dict)


def _param_flag(param: Parameter) -> str:
    """Return the resolved CLI flag string for a parameter."""
    if param.cli11.raw_cpp:
        return _flag_from_raw_cpp(param.cli11.raw_cpp) or f"--{param.name}"
    return param.cli11_flag


def _merge(src_subcmds: list[Subcommand], schema: Schema, dst: dict[str, MergedSubcommand]) -> None:
    for sub in src_subcmds:
        if sub.name not in dst:
            dst[sub.name] = MergedSubcommand(name=sub.name, description=sub.description)
        node = dst[sub.name]
        # Deduplicate by flag name: skip params whose flag is already present in the node.
        seen_flags = {_param_flag(p) for p in node.schema_params}
        for pname in sub.parameters:
            try:
                p = schema.param_by_name(pname)
                if not (p.mode == "struct-only" and not p.cli11.raw_cpp):
                    flag = _param_flag(p)
                    if flag not in seen_flags:
                        node.schema_params.append(p)
                        seen_flags.add(flag)
            except KeyError:
                pass
        _merge(sub.subcommands, schema, node.subcommands)


def build_tree(schemas: list[Schema]) -> dict[str, MergedSubcommand]:
    tree: dict[str, MergedSubcommand] = {}
    for schema in schemas:
        _merge(schema.subcommands, schema, tree)
    return tree


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------


def _flag_from_raw_cpp(raw_cpp: str) -> Optional[str]:
    m = re.search(r'"(--[A-Za-z0-9_,]+)"', raw_cpp)
    return m.group(1) if m else None


def _type_label(param: Parameter) -> Optional[str]:
    t = param.type
    if not t or t in ("struct-only",):
        return None
    if t in ("bool", "int", "unsigned", "float", "double", "string"):
        return t
    if t == "string_enum":
        return param.enum_cpp_type or "enum"
    # For complex C++ types (e.g. optional<int>), strip std:: namespace.
    # For opaque types (e.g. ocudulog::basic_levels used with raw_cpp), suppress.
    if param.cli11.raw_cpp and ("::" in t or "{" in t):
        return None
    return re.sub(r"\bstd::", "", t)


def _is_cpp_expr(value: str) -> bool:
    """Return True if value looks like a C++ expression rather than a plain default string."""
    return "::" in value or "{" in value or value.startswith("std::")


def _render_schema_param(param: Parameter, out: IO[str]) -> None:
    if param.cli11.raw_cpp:
        flag = _flag_from_raw_cpp(param.cli11.raw_cpp) or f"--{param.name}"
    else:
        flag = param.cli11_flag

    type_s = _type_label(param)
    type_part = f" (`{type_s}`)" if type_s else ""
    line = f"- **`{flag}`**{type_part}: {param.description}."

    extras: list[str] = []
    if param.default is not None and not _is_cpp_expr(str(param.default)):
        extras.append(f"Default: `{param.default}`")
    if param.range is not None:
        extras.append(f"Range: [{param.range.min}, {param.range.max}]")
    if param.allowed_values:
        vals = ", ".join(f"`{v.value}`" for v in param.allowed_values)
        extras.append(f"Allowed values: {vals}")
    if param.mode == "required":
        extras.append("**Required**")
    if param.mode == "auto-derived":
        extras.append("Auto-derived if omitted")

    if extras:
        line += " " + " | ".join(extras) + "."
    out.write(line + "\n")


def _render_inline_param(p: InlineParam, out: IO[str]) -> None:
    type_part = f" (`{p.type}`)" if p.type else ""
    line = f"- **`{p.flag}`**{type_part}: {p.description}."
    extras: list[str] = []
    if p.default is not None:
        extras.append(f"Default: `{p.default}`")
    if p.range is not None:
        extras.append(f"Range: [{p.range.min}, {p.range.max}]")
    if extras:
        line += " " + " | ".join(extras) + "."
    out.write(line + "\n")


def _render_node(node: MergedSubcommand, out: IO[str], level: int) -> None:
    heading = "#" * level
    out.write(f"\n{heading} `{node.name}`\n\n")
    desc = node.description
    if desc and desc[0].islower():
        desc = desc[0].upper() + desc[1:]
    out.write(f"{desc}.\n")

    if node.schema_params:
        out.write("\n")
        for param in node.schema_params:
            _render_schema_param(param, out)

    for child in node.subcommands.values():
        _render_node(child, out, level + 1)


def render(title: str, root_params: list[InlineParam], tree: dict[str, MergedSubcommand], out: IO[str]) -> None:
    out.write(f"# {title}\n")
    if root_params:
        out.write("\n")
        for p in root_params:
            _render_inline_param(p, out)
    for node in tree.values():
        _render_node(node, out, level=2)
    out.write("\n")


# ---------------------------------------------------------------------------
# App spec loading
# ---------------------------------------------------------------------------


def _load_inline_param(d: dict) -> InlineParam:
    range_ = None
    if "range" in d:
        r = d["range"]
        range_ = Range(min=r["min"], max=r["max"])
    return InlineParam(
        flag=str(d["flag"]),
        description=d["description"],
        type=d.get("type"),
        default=str(d["default"]) if "default" in d else None,
        range=range_,
    )


def load_app_spec(path: Path) -> tuple[str, list[InlineParam], list[Schema]]:
    with open(path) as f:
        d = yaml.safe_load(f)

    title = d.get("title", "Application Configuration")
    root_params = [_load_inline_param(p) for p in d.get("root_params", [])]

    base = path.parent
    schemas: list[Schema] = []
    for rel in d.get("schemas", []):
        schema_path = base / rel
        try:
            schemas.append(load_schema(schema_path))
        except Exception as exc:
            print(f"warning: {schema_path}: {exc}", file=sys.stderr)

    return title, root_params, schemas


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def _is_schema_file(path: Path) -> bool:
    """Return True if path is a schema YAML (has schema_version key) rather than an app spec."""
    with open(path) as f:
        d = yaml.safe_load(f)
    return isinstance(d, dict) and "schema_version" in d


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a Markdown CLI reference. "
        "Accepts an app composition spec YAML (with 'schemas:' list) "
        "or a single schema YAML (with 'schema_version:')."
    )
    parser.add_argument("input", type=Path, metavar="FILE", help="App spec YAML or schema YAML")
    parser.add_argument("-o", "--output", type=Path, metavar="FILE", help="Output Markdown file (default: stdout)")
    args = parser.parse_args()

    try:
        if _is_schema_file(args.input):
            schema = load_schema(args.input)
            writer = lambda out: render_schema(schema, out, schema_level=1)
        else:
            title, root_params, schemas = load_app_spec(args.input)
            tree = build_tree(schemas)
            writer = lambda out: render(title, root_params, tree, out)
    except Exception as exc:
        print(f"error: {args.input}: {exc}", file=sys.stderr)
        return 1

    if args.output:
        with args.output.open("w") as f:
            writer(f)
        print(f"wrote {args.output}", file=sys.stderr)
        # Warn if the file is nearly empty (schema with no structured content).
        text = args.output.read_text().strip()
        if text.count("\n") < 2:
            print(
                f"warning: output contains only a title. "
                f"If you passed a schema file, its CLI may be defined in raw C++ preambles "
                f"and won't appear here. Pass an app spec YAML (e.g. apps/config/gnb.app.yaml) "
                f"to generate a full reference.",
                file=sys.stderr,
            )
    else:
        writer(sys.stdout)

    return 0


if __name__ == "__main__":
    sys.exit(main())
