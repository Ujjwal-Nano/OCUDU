#!/usr/bin/env python3

# SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI

"""Generate C++ config headers and CLI11 schema files from YAML schema definitions."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

import jinja2

_TOOLS_DIR = Path(__file__).parent
_REPO_ROOT = _TOOLS_DIR.parent.parent
_TEMPLATES_DIR = _TOOLS_DIR / "templates"

sys.path.insert(0, str(_TOOLS_DIR))
from schema import Parameter, Schema, Subcommand, load_schema  # noqa: E402


# ---------------------------------------------------------------------------
# C++ rendering helpers (keep formatting logic in Python, not templates)
# ---------------------------------------------------------------------------

def _render_option(param: Parameter, config_var: str, app_var: str = "app") -> str:
    """Return the complete add_option(...) statement for one parameter, no leading indent.

    app_var is the CLI::App variable to use (e.g. "app", "*layers_subcmd").
    """
    if param.cli11.raw_cpp:
        return param.cli11.raw_cpp.strip()

    flag = param.cli11_flag
    chains: list[str] = []

    if param.type == "string_enum":
        values_str = ", ".join(f'"{av.value}"' for av in param.allowed_values)
        body_lines: list[str] = []
        for i, av in enumerate(param.allowed_values):
            kw = "if" if i == 0 else "} else if"
            body_lines.append(f"        {kw} (value == \"{av.value}\") {{")
            body_lines.append(f"          {config_var}.{param.name} = {av.cpp};")
        body_lines.append("        }")
        body = "\n".join(body_lines)
        call = (
            f'add_option_function<std::string>(\n'
            f'      {app_var},\n'
            f'      "{flag}",\n'
            f'      [&{config_var}](const std::string& value) {{\n'
            f'{body}\n'
            f'      }},\n'
            f'      "{param.description}")'
        )
        chains.append("->capture_default_str()")
        chains.append(f"->check(CLI::IsMember({{{values_str}}}, CLI::ignore_case))")
    else:
        member = param.cli11.member if param.cli11.member else param.name
        call = f'add_option({app_var}, "{flag}", {config_var}.{member}, "{param.description}")'

    if param.cli11.always_capture:
        chains.append("->always_capture_default()")
    elif param.cli11.capture:
        chains.append("->capture_default_str()")

    if param.range is not None:
        chains.append(f"->check(CLI::Range({param.range.min}, {param.range.max}))")

    if param.mode == "required":
        chains.append("->required()")

    if chains:
        return call + "\n    " + "\n    ".join(chains) + ";"
    return call + ";"


def _render_subcmd_fn(subcmd: Subcommand, schema: Schema) -> str:
    """Return the static configure_cli11_<name>_args function body.

    Nested subcommands are rendered inline (not as separate static functions).
    """
    config_var = f"config.{subcmd.member}" if subcmd.member else "config"
    lines = [f"static void configure_cli11_{subcmd.name}_args(CLI::App& app, {schema.name}& config)", "{"]

    # Direct parameters of this subcommand go flat on the passed app.
    for pname in subcmd.parameters:
        p = schema.param_by_name(pname)
        if p.mode in ("struct-only", "doc-only") and not p.cli11.raw_cpp:
            continue
        for line in _render_option(p, config_var, app_var="app").splitlines():
            lines.append("  " + line)

    # Nested subcommands are created inline and their options added to their own app.
    for nested in subcmd.subcommands:
        nested_config_var = f"config.{nested.member}" if nested.member else "config"
        cfg = "->configurable()" if nested.configurable else ""
        lines.append(
            f'  CLI::App* {nested.name}_subcmd = add_subcommand(app, "{nested.name}", "{nested.description}"){cfg};'
        )
        for pname in nested.parameters:
            p = schema.param_by_name(pname)
            if p.mode in ("struct-only", "doc-only") and not p.cli11.raw_cpp:
                continue
            for line in _render_option(p, nested_config_var, app_var=f"*{nested.name}_subcmd").splitlines():
                lines.append("  " + line)
        if nested.parse_complete_callback:
            cb = nested.parse_complete_callback.rstrip()
            indented = "\n".join("    " + l for l in cb.splitlines())
            lines.append(f"  {nested.name}_subcmd->parse_complete_callback([&config]() {{")
            lines.append(indented)
            lines.append("  });")

    lines.append("}")
    return "\n".join(lines)


def _all_subcmd_params(subcommands: list[Subcommand]) -> set[str]:
    """Collect all parameter names claimed at any nesting level of subcommands."""
    claimed: set[str] = set()
    for sub in subcommands:
        claimed.update(sub.parameters)
        claimed.update(_all_subcmd_params(sub.subcommands))
    return claimed


def _render_top_level_fn(schema: Schema) -> str:
    """Return the public configure function body."""
    fq = f"{schema.namespace}::{schema.cli11.function}"
    lines = [f"void {fq}(CLI::App& app, {schema.name}& config)", "{"]

    top = "app"
    if schema.cli11.top_subcommand:
        ts = schema.cli11.top_subcommand
        lines.append(f'  CLI::App* top_subcmd = add_subcommand(app, "{ts.name}", "{ts.description}")->configurable();')
        top = "top_subcmd"

    # Collect all params claimed at any depth; the rest go flat on the top-level app.
    subcmd_params = _all_subcmd_params(schema.subcommands)

    for param in schema.parameters:
        if param.name in subcmd_params or (param.mode in ("struct-only", "doc-only") and not param.cli11.raw_cpp):
            continue
        for line in _render_option(param, "config").splitlines():
            lines.append("  " + line)

    for sub in schema.subcommands:
        cfg = "->configurable()" if sub.configurable else ""
        lines.append(
            f'  CLI::App* {sub.name}_subcmd = add_subcommand({top}, "{sub.name}", "{sub.description}"){cfg};'
        )
        lines.append(f"  configure_cli11_{sub.name}_args(*{sub.name}_subcmd, config);")
        if sub.parse_complete_callback:
            cb = sub.parse_complete_callback.rstrip()
            indented = "\n".join("    " + l for l in cb.splitlines())
            lines.append(f"  {sub.name}_subcmd->parse_complete_callback([&config]() {{")
            lines.append(indented)
            lines.append("  });")

    if schema.cli11.post_configure_raw:
        for line in schema.cli11.post_configure_raw.strip().splitlines():
            lines.append("  " + line)

    lines.append("}")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Jinja2 environment + rendering
# ---------------------------------------------------------------------------

def _make_env(schema: Schema, schema_rel_path: str) -> jinja2.Environment:
    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(str(_TEMPLATES_DIR)),
        trim_blocks=True,
        lstrip_blocks=True,
        keep_trailing_newline=True,
        undefined=jinja2.StrictUndefined,
    )
    env.filters["basename"] = lambda p: Path(p).name

    # Expose Python rendering helpers to templates
    env.globals.update(
        schema=schema,
        schema_rel_path=schema_rel_path,
        render_subcmd_fns=lambda: "\n\n".join(_render_subcmd_fn(s, schema) for s in schema.subcommands),
        render_top_level_fn=lambda: _render_top_level_fn(schema),
    )
    return env


def _render_all(schema: Schema, schema_rel_path: str) -> dict[str, str]:
    env = _make_env(schema, schema_rel_path)
    outputs = {
        schema.outputs.cli11_header: env.get_template("cli11_schema_h.j2").render(),
    }
    if schema.generate_header:
        outputs[schema.outputs.header] = env.get_template("config_header.h.j2").render()
    if schema.generate_cli11_source:
        outputs[schema.outputs.cli11_source] = env.get_template("cli11_schema_cpp.j2").render()
    return outputs


# ---------------------------------------------------------------------------
# File I/O
# ---------------------------------------------------------------------------

def _clang_format(content: str, path: Path) -> str:
    """Run clang-format-18 on content (only for .h/.cpp files)."""
    if path.suffix not in (".h", ".cpp"):
        return content
    result = subprocess.run(
        ["clang-format-18", f"--assume-filename={path.name}", "-"],
        input=content,
        capture_output=True,
        text=True,
    )
    return result.stdout if result.returncode == 0 else content


def _write_if_changed(path: Path, content: str, check: bool, changed: list[Path]) -> None:
    content = _clang_format(content.lstrip("\n"), path)
    if path.exists() and path.read_text() == content:
        return
    if check:
        changed.append(path)
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        print(f"  wrote {path.relative_to(_REPO_ROOT)}")


def generate(schema_path: Path, check: bool = False) -> list[Path]:
    schema = load_schema(schema_path)
    schema_rel = str(schema_path.relative_to(_REPO_ROOT))
    outputs = _render_all(schema, schema_rel)
    changed: list[Path] = []
    for rel_path, content in outputs.items():
        _write_if_changed(_REPO_ROOT / rel_path, content, check, changed)
    return changed


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description="Generate C++ from a YAML config schema.")
    parser.add_argument("schema", nargs="+", type=Path, help="Path(s) to .schema.yaml file(s)")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Exit non-zero if any output differs from what would be generated (CI mode).",
    )
    args = parser.parse_args()

    all_changed: list[Path] = []
    for schema_path in args.schema:
        schema_path = schema_path.resolve()
        print(f"Processing {schema_path.name}...")
        all_changed.extend(generate(schema_path, check=args.check))

    if args.check and all_changed:
        print("\nOut-of-sync generated files (re-run generate.py):")
        for p in all_changed:
            print(f"  {p.relative_to(_REPO_ROOT)}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
