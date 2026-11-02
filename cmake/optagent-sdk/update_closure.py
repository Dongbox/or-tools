"""Recompute the pinned CP-SAT source closure from upstream Bazel declarations.

Read only by default. Use --write to update the maintained CMake manifest after
reviewing a dependency upgrade. Platform select branches are unioned, retaining
all supported platforms rather than the host's subset. No upstream build runs.
"""

from __future__ import annotations

import argparse
import ast
from pathlib import Path
import re
import subprocess

_REVISION = "551ad10d94835c99e5e1e684500d3db398c0e345"
_MANIFEST = Path(__file__).with_name("sources.cmake")


def _canonical(label, package):
    if label.startswith("@"):
        return label
    if label.startswith("//"):
        return label if ":" in label else label + ":" + label.rsplit("/", 1)[-1]
    return "//" + package + ":" + label.removeprefix(":")


def _closure(root):
    rules, parsed = {}, set()

    def parse(package):
        if package in parsed:
            return
        path = root / package / "BUILD.bazel"
        tree = ast.parse(path.read_text())

        def recorder(kind):
            def record(*_args, **kwargs):
                if "name" in kwargs:
                    rules["//" + package + ":" + kwargs["name"]] = (package, kind, kwargs)

            return record

        def select(choices, **_kwargs):
            return list(dict.fromkeys(item for branch in choices.values() for item in branch))

        def glob(patterns, exclude=(), **_kwargs):
            excluded = {
                str(p.relative_to(root / package)) for pattern in exclude for p in (root / package).glob(pattern)
            }
            return sorted(
                {
                    str(p.relative_to(root / package))
                    for pattern in patterns
                    for p in (root / package).glob(pattern)
                    if p.is_file()
                }
                - excluded
            )

        environment = {"select": select, "glob": glob, "load": lambda *_a, **_k: None, "__builtins__": {}}
        for node in ast.walk(tree):
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id not in environment:
                environment[node.func.id] = recorder(node.func.id)
        # Only the exact verified upstream revision is accepted by main().
        exec(compile(tree, str(path), "exec"), environment)
        parsed.add(package)

    pending = ["//ortools/sat:cp_model_solver", "//ortools/sat:cp_model"]
    seen, external = set(), set()
    while pending:
        label = pending.pop()
        if label in seen:
            continue
        seen.add(label)
        if label.startswith("@"):
            external.add(label)
            continue
        package, name = label[2:].split(":")
        parse(package)
        if label not in rules:
            if "." in name:
                continue
            raise ValueError(f"missing upstream rule: {label}")
        for attribute in ("deps", "implementation_deps", "exports", "actual"):
            values = rules[label][2].get(attribute, [])
            if isinstance(values, str):
                values = [values]
            pending.extend(_canonical(value, package) for value in values)
    result = {"SOURCES": set(), "HEADERS": set(), "PROTOS": set(), "ABSL_TARGETS": set()}
    for label in sorted(seen & rules.keys()):
        package, _, rule = rules[label]
        for attribute in ("srcs", "hdrs", "textual_hdrs"):
            for value in rule.get(attribute, []):
                if value.startswith(":"):
                    raise ValueError(f"unhandled generated source: {label} {value}")
                path = value[2:].replace(":", "/") if value.startswith("//") else package + "/" + value
                kind = "SOURCES" if path.endswith(".cc") else "PROTOS" if path.endswith(".proto") else "HEADERS"
                result[kind].add(path)
    special = {
        "algorithm:container": "algorithm_container",
        "flags:flag": "flags",
        "flags:parse": "flags_parse",
        "flags:usage": "flags_usage",
        "random": "random_random",
        "log:globals": "log_globals",
        "log:initialize": "log_initialize",
    }
    for label in external:
        if not label.startswith("@abseil-cpp//absl/"):
            if label not in {"@protobuf", "@zlib", "@bzip2//:bz2"}:
                raise ValueError(f"unhandled external dependency: {label}")
            continue
        tail = label.removeprefix("@abseil-cpp//absl/")
        target = special.get(tail, tail.split(":")[-1])
        if tail.startswith("random:"):
            target = "random_" + target
        result["ABSL_TARGETS"].add("absl::" + target)
    return result


def _main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("upstream", type=Path)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    revision = subprocess.check_output(["git", "-C", str(args.upstream), "rev-parse", "HEAD"], text=True).strip()
    if subprocess.run(["git", "-C", str(args.upstream), "merge-base", "--is-ancestor", _REVISION, revision], check=False).returncode:
        parser.error("SDK checkout does not descend from the audited upstream revision")
    subprocess.run(
        ["git", "-C", str(args.upstream), "diff", "--exit-code", "HEAD", "--", "ortools"],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    closure = _closure(args.upstream)
    content = f"# CP-SAT closure of cp_model_solver and cp_model at {_REVISION}.\n"
    for kind, paths in closure.items():
        content += f"set(OPTAGENT_CPSAT_{kind}\n" + "".join("  " + path + "\n" for path in sorted(paths)) + ")\n"
    if args.write:
        _MANIFEST.write_text(content)
    else:
        actual = _MANIFEST.read_text()
        for kind, paths in closure.items():
            match = re.search(r"set\(OPTAGENT_CPSAT_" + kind + r"\s+([^)]*)\)", actual)
            if match is None or set(match[1].split()) != paths:
                raise SystemExit(f"CP-SAT manifest differs: {kind}")
    print("CP-SAT closure verified: " + ", ".join(f"{len(paths)} {kind.lower()}" for kind, paths in closure.items()))


if __name__ == "__main__":
    _main()
