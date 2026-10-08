#!/usr/bin/env python3
"""Verify the committed RCC/RCC++ deterministic artifact corpus."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tests" / "golden.json"
TARGETS = ("i686-unknown-rinos", "x86_64-unknown-rinos")
KINDS = ("ro", "rin", "rll", "drv")
LANGUAGES = {
    "C17": (".c", "rcc"),
    "C++20": (".cpp", "rcc++"),
}


class GoldenError(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def source_path(spelling: object, language: str) -> Path:
    if not isinstance(spelling, str) or not spelling or "\\" in spelling:
        raise GoldenError("source must be a non-empty POSIX relative path")
    relative = PurePosixPath(spelling)
    if (relative.is_absolute() or
            any(part in ("", ".", "..") for part in relative.parts)):
        raise GoldenError(f"source escapes repository: {spelling}")
    source = ROOT.joinpath(*relative.parts).resolve()
    try:
        source.relative_to(ROOT)
    except ValueError as error:
        raise GoldenError(f"source escapes repository: {spelling}") from error
    if source.suffix != LANGUAGES[language][0] or not source.is_file():
        raise GoldenError(f"source is not a {language} fixture: {spelling}")
    return source


def load_cases() -> list[dict[str, object]]:
    try:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise GoldenError(f"cannot read manifest: {error}") from error
    if not isinstance(document, dict) or document.get("schema") != 1:
        raise GoldenError("golden manifest schema must be 1")
    cases = document.get("cases")
    if not isinstance(cases, list) or not cases:
        raise GoldenError("golden manifest must contain cases")
    result: list[dict[str, object]] = []
    seen: set[str] = set()
    for case in cases:
        if not isinstance(case, dict):
            raise GoldenError("golden case must be an object")
        if set(case) != {"id", "language", "source", "flags", "sha256"}:
            raise GoldenError("golden case keys are invalid")
        case_id = case["id"]
        language = case["language"]
        if (not isinstance(case_id, str) or not case_id or case_id in seen or
                not isinstance(language, str) or language not in LANGUAGES):
            raise GoldenError("golden case id/language is invalid")
        seen.add(case_id)
        flags = case["flags"]
        if (not isinstance(flags, list) or
                any(not isinstance(flag, str) for flag in flags)):
            raise GoldenError(f"golden flags are invalid: {case_id}")
        hashes = case["sha256"]
        if not isinstance(hashes, dict) or set(hashes) != set(TARGETS):
            raise GoldenError(f"golden targets are invalid: {case_id}")
        for target in TARGETS:
            values = hashes[target]
            if (not isinstance(values, dict) or set(values) != set(KINDS) or
                    any(not isinstance(values[key], str) or len(values[key]) != 64
                        for key in KINDS)):
                raise GoldenError(f"golden hashes are invalid: {case_id}/{target}")
        result.append({
            "id": case_id,
            "language": language,
            "source": source_path(case["source"], language),
            "flags": flags,
            "sha256": hashes,
        })
    return result


def resolve_tool(argument: str | None, default_name: str) -> Path:
    path = Path(argument) if argument else Path("build") / default_name
    if not path.is_absolute():
        path = ROOT / path
    if not path.is_file():
        raise GoldenError(f"compiler not found: {path}")
    return path.resolve()


def run_case(case: dict[str, object], tools: dict[str, Path], timeout: int,
             temporary: Path, update: bool = False
             ) -> dict[str, dict[str, str]]:
    language = str(case["language"])
    tool = tools[LANGUAGES[language][1]]
    source = case["source"]
    flags = [str(flag) for flag in case["flags"]]
    hashes = case["sha256"]
    assert isinstance(source, Path) and isinstance(hashes, dict)
    source_argument = source.relative_to(ROOT).as_posix()
    updated_hashes: dict[str, dict[str, str]] = {}
    for target in TARGETS:
        expected = hashes[target]
        assert isinstance(expected, dict)
        updated_hashes[target] = {}
        for kind in KINDS:
            observed: list[str] = []
            for run_index in (1, 2):
                output = (temporary / str(case["id"]) / target /
                          f"{kind}-{run_index}.{kind}")
                output.parent.mkdir(parents=True, exist_ok=True)
                command = [str(tool), "--target", target, *flags]
                if kind == "ro":
                    command.append("-c")
                elif kind == "rin":
                    command.append("--emit-unsigned-v3")
                elif kind == "rll":
                    command.extend(("-shared", "--emit-unsigned-v3"))
                else:
                    command.extend(("-driver", "--emit-unsigned-v3"))
                command.extend(["-o", str(output), source_argument])
                completed = subprocess.run(
                    command, cwd=ROOT, capture_output=True, text=True,
                    timeout=timeout, check=False,
                )
                if (completed.returncode != 0 or not output.is_file() or
                        output.stat().st_size == 0):
                    detail = (completed.stdout + completed.stderr).strip()[-2000:]
                    raise GoldenError(
                        f"{case['id']}/{target}/{kind}: compiler rejected fixture: {detail}")
                observed.append(sha256(output))
            if observed[0] != observed[1]:
                raise GoldenError(
                    f"{case['id']}/{target}/{kind}: repeated output differs")
            if observed[0] != expected[kind] and not update:
                raise GoldenError(
                    f"{case['id']}/{target}/{kind}: golden mismatch "
                    f"expected {expected[kind]}, observed {observed[0]}")
            updated_hashes[target][kind] = observed[0]
            action = "UPDATE" if update and observed[0] != expected[kind] else "PASS"
            print(f"{action} {case['id']} [{target}] {kind} {observed[0]}")
    return updated_hashes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rcc")
    parser.add_argument("--rccxx")
    parser.add_argument("--timeout", type=int, default=60)
    parser.add_argument("--update", action="store_true",
                        help="regenerate hashes after deterministic double builds")
    args = parser.parse_args()
    if not 1 <= args.timeout <= 300:
        print("ERROR: timeout must be in 1..300")
        return 2
    try:
        cases = load_cases()
        tools = {
            "rcc": resolve_tool(args.rcc, "rcc"),
            "rcc++": resolve_tool(args.rccxx, "rcc++"),
        }
        (ROOT / "build").mkdir(parents=True, exist_ok=True)
        updated_hashes: dict[str, dict[str, dict[str, str]]] = {}
        with tempfile.TemporaryDirectory(prefix="rcc-golden-",
                                          dir=ROOT / "build") as directory:
            for case in cases:
                updated_hashes[str(case["id"])] = run_case(
                    case, tools, args.timeout, Path(directory), args.update)
        if args.update:
            document = json.loads(MANIFEST.read_text(encoding="utf-8"))
            for case in document["cases"]:
                case["sha256"] = updated_hashes[case["id"]]
            rendered = json.dumps(document, indent=2)
            for case in document["cases"]:
                flags = case["flags"]
                if not flags:
                    continue
                expanded_flags = (
                    '      "flags": [\n' +
                    ",\n".join(
                        f"        {json.dumps(flag)}" for flag in flags) +
                    "\n      ]")
                compact_flags = (
                    '      "flags": [' +
                    ", ".join(json.dumps(flag) for flag in flags) + "]")
                if expanded_flags not in rendered:
                    raise GoldenError(
                        f"cannot preserve manifest formatting: {case['id']}")
                rendered = rendered.replace(expanded_flags, compact_flags, 1)
            MANIFEST.write_text(rendered + "\n", encoding="utf-8")
    except (GoldenError, OSError, subprocess.SubprocessError) as error:
        print(f"ERROR: {error}")
        return 1
    result = "regenerated" if args.update else "passed"
    print(f"RCC golden artifacts: {len(cases)} case(s) {result}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
