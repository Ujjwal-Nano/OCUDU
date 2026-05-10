#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI

"""Generate a Markdown reference document from one or more YAML schema files.

Usage:
    # Single schema — schema is the document root (#), subcommands are ##
    python3 tools/config_gen/doc_schema.py apps/config/schemas/metrics_config.schema.yaml

    # Multiple schemas — each schema is ##, subcommands are ###
    python3 tools/config_gen/doc_schema.py apps/config/schemas/*.schema.yaml -o docs/config_reference.md
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import IO, Optional

sys.path.insert(0, str(Path(__file__).parent))
from schema import Parameter, Schema, Subcommand, load_schema


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _flag_from_raw_cpp(raw_cpp: str) -> Optional[str]:
    """Extract the first '--flag' string literal from a raw_cpp block."""
    m = re.search(r'"(--[A-Za-z0-9_,]+)"', raw_cpp)
    return m.group(1) if m else None


def _type_label(param: Parameter) -> Optional[str]:
    t = param.type
    if not t or t == "struct-only":
        return None
    if t in ("bool", "int", "unsigned", "float", "double", "string"):
        return t
    if t == "string_enum":
        return param.enum_cpp_type or "enum"
    # optional<T>, vector<T>, etc. — drop std:: and leave angle brackets as-is
    return re.sub(r"\bstd::", "", t)


def _render_param(param: Parameter, out: IO[str]) -> None:
    """Emit one bullet point for a parameter."""
    # Pure struct-only with no CLI counterpart — skip.
    if param.mode == "struct-only" and not param.cli11.raw_cpp:
        return

    # Resolve the CLI flag name.
    if param.cli11.raw_cpp:
        flag = _flag_from_raw_cpp(param.cli11.raw_cpp) or f"--{param.name}"
    else:
        flag = param.cli11_flag

    # Build the bullet: **`--flag`** (`type`): description.
    type_s = _type_label(param)
    type_part = f" (`{type_s}`)" if type_s else ""
    line = f"- **`{flag}`**{type_part}: {param.description}."

    # Collect trailing annotations.
    extras: list[str] = []
    if param.default is not None:
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


def _all_subcmd_param_names(subcommands: list[Subcommand]) -> set[str]:
    names: set[str] = set()
    for sub in subcommands:
        names.update(sub.parameters)
        names.update(_all_subcmd_param_names(sub.subcommands))
    return names


def _has_visible_params(sub: Subcommand, schema: Schema) -> bool:
    """Return True if this subcommand (or any descendant) has at least one visible parameter."""
    for name in sub.parameters:
        try:
            p = schema.param_by_name(name)
            if not (p.mode == "struct-only" and not p.cli11.raw_cpp):
                return True
        except KeyError:
            pass
    return any(_has_visible_params(nested, schema) for nested in sub.subcommands)


def _schema_has_content(schema: Schema) -> bool:
    """Return True if the schema has anything worth documenting."""
    claimed = _all_subcmd_param_names(schema.subcommands)
    top_params = [p for p in schema.parameters if p.name not in claimed]
    visible_top = [p for p in top_params if not (p.mode == "struct-only" and not p.cli11.raw_cpp)]
    if visible_top:
        return True
    return any(_has_visible_params(sub, schema) for sub in schema.subcommands)


def _render_subcommand(
    sub: Subcommand,
    schema: Schema,
    out: IO[str],
    level: int,
) -> None:
    heading = "#" * level
    out.write(f"\n{heading} `{sub.name}`\n\n")
    out.write(f"{sub.description}.\n")

    params = []
    for name in sub.parameters:
        try:
            params.append(schema.param_by_name(name))
        except KeyError:
            pass  # referenced param not in schema parameters list

    visible = [p for p in params if not (p.mode == "struct-only" and not p.cli11.raw_cpp)]
    if visible:
        out.write("\n")
        for param in visible:
            _render_param(param, out)

    for nested in sub.subcommands:
        _render_subcommand(nested, schema, out, level + 1)


# ---------------------------------------------------------------------------
# Top-level renderer
# ---------------------------------------------------------------------------


def render_schema(schema: Schema, out: IO[str], schema_level: int = 1) -> None:
    """Render one schema.  schema_level controls the heading depth of the schema title:
    1 → the schema is the document root (#), subcommands start at ##.
    2 → the schema is a section (##), subcommands start at ###.
    """
    desc = schema.description
    if desc and desc[0].islower():
        desc = desc[0].upper() + desc[1:]
    heading = "#" * schema_level
    out.write(f"{heading} {desc}\n")

    claimed = _all_subcmd_param_names(schema.subcommands)
    top_params = [p for p in schema.parameters if p.name not in claimed]
    visible_top = [p for p in top_params if not (p.mode == "struct-only" and not p.cli11.raw_cpp)]

    if visible_top:
        out.write("\n")
        for param in visible_top:
            _render_param(param, out)

    for sub in schema.subcommands:
        _render_subcommand(sub, schema, out, schema_level + 1)

    out.write("\n")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a Markdown config reference from YAML schema files."
    )
    parser.add_argument("schemas", nargs="+", type=Path, metavar="SCHEMA", help="Schema YAML file(s)")
    parser.add_argument("-o", "--output", type=Path, metavar="FILE", help="Output Markdown file (default: stdout)")
    args = parser.parse_args()

    schemas = []
    for path in args.schemas:
        try:
            schemas.append(load_schema(path))
        except Exception as exc:
            print(f"error: {path}: {exc}", file=sys.stderr)
            return 1

    def write_all(out: IO[str]) -> None:
        if len(schemas) == 1:
            render_schema(schemas[0], out, schema_level=1)
        else:
            out.write("# Configuration Reference\n\n")
            for schema in schemas:
                if not schema.doc_exclude and _schema_has_content(schema):
                    render_schema(schema, out, schema_level=2)

    if args.output:
        with args.output.open("w") as f:
            write_all(f)
        print(f"wrote {args.output}", file=sys.stderr)
    else:
        write_all(sys.stdout)

    return 0


if __name__ == "__main__":
    sys.exit(main())
