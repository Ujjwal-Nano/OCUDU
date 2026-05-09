# SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI

"""Schema dataclasses and YAML loading for the CLI config generator."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import yaml


@dataclass
class Cli11Param:
    """CLI11-specific options for a parameter."""

    flag: Optional[str] = None
    always_capture: bool = False
    capture: bool = False
    raw_cpp: Optional[str] = None
    auto_sentinel: Optional[str] = None
    # Override the C++ member access path (relative to the subcommand's config_var).
    # E.g. "f1ap.filename" → config_var.f1ap.filename instead of config_var.<name>.
    member: Optional[str] = None


@dataclass
class AllowedValue:
    value: str
    cpp: str


@dataclass
class Range:
    min: object  # int, float, or str expression
    max: object


@dataclass
class Parameter:
    name: str
    description: str
    type: str
    mode: str = "default"
    default: Optional[str] = None
    range: Optional[Range] = None
    enum_cpp_type: Optional[str] = None
    allowed_values: list[AllowedValue] = field(default_factory=list)
    cli11: Cli11Param = field(default_factory=Cli11Param)

    @property
    def cli11_flag(self) -> str:
        return self.cli11.flag if self.cli11.flag else f"--{self.name}"

    @property
    def cpp_type(self) -> str:
        """Return the C++ type string for the header."""
        mapping = {
            "bool": "bool",
            "int": "int",
            "unsigned": "unsigned",
            "float": "float",
            "double": "double",
            "string": "std::string",
            "string_enum": self.enum_cpp_type or "/* unknown_enum */",
        }
        if self.type in mapping:
            return mapping[self.type]
        # Pass through for complex types like optional<int>, vector<T>, etc.
        t = self.type
        t = re.sub(r"\bstring\b", "std::string", t)
        t = re.sub(r"\boptional<", "std::optional<", t)
        t = re.sub(r"\bvector<", "std::vector<", t)
        return t


@dataclass
class Subcommand:
    name: str
    description: str
    configurable: bool = True
    parameters: list[str] = field(default_factory=list)
    subcommands: list[Subcommand] = field(default_factory=list)
    parse_complete_callback: Optional[str] = None
    # When set, parameters in this subcommand access config.<member>.<param> instead of config.<param>.
    member: Optional[str] = None


@dataclass
class Outputs:
    header: str
    cli11_source: str
    cli11_header: str


@dataclass
class TopSubcommand:
    name: str
    description: str


@dataclass
class Cli11Config:
    function: str
    top_subcommand: Optional[TopSubcommand] = None
    # Raw C++ appended verbatim at the end of the top-level configure function body.
    post_configure_raw: Optional[str] = None


@dataclass
class Schema:
    schema_version: int
    name: str
    namespace: str
    description: str
    outputs: Outputs
    parameters: list[Parameter]
    subcommands: list[Subcommand]
    cli11: Cli11Config
    header_includes: list[str] = field(default_factory=list)
    cli11_extra_includes: list[str] = field(default_factory=list)
    struct_extra: Optional[str] = None
    # Extra #include lines added to the generated CLI11 header (after CLI/CLI11.hpp).
    cli11_header_extra_includes: list[str] = field(default_factory=list)
    # Raw C++ appended after the configure function declaration in the generated CLI11 header.
    cli11_header_extra: Optional[str] = None
    # Raw C++ inserted between the generated subcommand helpers and the top-level configure function.
    cli11_source_preamble: Optional[str] = None
    # Raw C++ appended after the top-level configure function definition in the generated CLI11 source.
    cli11_source_extra: Optional[str] = None
    # When False, skip generating the config struct header (use for multi-struct headers).
    generate_header: bool = True
    # When False, skip generating the CLI11 source .cpp (use when the source is too complex to express in the schema).
    generate_cli11_source: bool = True

    @property
    def namespace_parts(self) -> list[str]:
        return self.namespace.split("::")

    def param_by_name(self, name: str) -> Parameter:
        for p in self.parameters:
            if p.name == name:
                return p
        raise KeyError(f"Parameter '{name}' not found in schema '{self.name}'")


def _load_cli11_param(d: dict) -> Cli11Param:
    if d is None:
        return Cli11Param()
    return Cli11Param(
        flag=d.get("flag"),
        always_capture=d.get("always_capture", False),
        capture=d.get("capture", False),
        raw_cpp=d.get("raw_cpp"),
        auto_sentinel=d.get("auto_sentinel"),
        member=d.get("member"),
    )


def _load_parameter(d: dict) -> Parameter:
    range_ = None
    if "range" in d:
        r = d["range"]
        range_ = Range(min=r["min"], max=r["max"])
    allowed = [AllowedValue(value=v["value"], cpp=v["cpp"]) for v in d.get("allowed_values", [])]
    return Parameter(
        name=d["name"],
        description=d["description"],
        type=d["type"],
        mode=d.get("mode", "default"),
        default=d.get("default"),
        range=range_,
        enum_cpp_type=d.get("enum_cpp_type"),
        allowed_values=allowed,
        cli11=_load_cli11_param(d.get("cli11")),
    )


def _load_subcommand(d: dict) -> Subcommand:
    nested = [_load_subcommand(s) for s in d.get("subcommands", [])]
    return Subcommand(
        name=d["name"],
        description=d["description"],
        configurable=d.get("configurable", True),
        parameters=d.get("parameters", []),
        subcommands=nested,
        parse_complete_callback=d.get("parse_complete_callback"),
        member=d.get("member"),
    )


def load_schema(path: Path) -> Schema:
    with open(path) as f:
        d = yaml.safe_load(f)

    outputs = Outputs(
        header=d["outputs"]["header"],
        cli11_source=d["outputs"]["cli11_source"],
        cli11_header=d["outputs"]["cli11_header"],
    )

    cli11_d = d["cli11"]
    top_sub = None
    if "top_subcommand" in cli11_d:
        ts = cli11_d["top_subcommand"]
        top_sub = TopSubcommand(name=ts["name"], description=ts["description"])
    cli11_cfg = Cli11Config(
        function=cli11_d["function"],
        top_subcommand=top_sub,
        post_configure_raw=cli11_d.get("post_configure_raw"),
    )

    return Schema(
        schema_version=d.get("schema_version", 1),
        name=d["name"],
        namespace=d["namespace"],
        description=d["description"],
        outputs=outputs,
        parameters=[_load_parameter(p) for p in d.get("parameters", [])],
        subcommands=[_load_subcommand(s) for s in d.get("subcommands", [])],
        cli11=cli11_cfg,
        header_includes=d.get("header_includes", []),
        cli11_extra_includes=d.get("cli11_extra_includes", []),
        struct_extra=d.get("struct_extra"),
        cli11_header_extra_includes=d.get("cli11_header_extra_includes", []),
        cli11_header_extra=d.get("cli11_header_extra"),
        cli11_source_preamble=d.get("cli11_source_preamble"),
        cli11_source_extra=d.get("cli11_source_extra"),
        generate_header=d.get("generate_header", True),
        generate_cli11_source=d.get("generate_cli11_source", True),
    )
