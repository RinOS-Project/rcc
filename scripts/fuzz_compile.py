#!/usr/bin/env python3
"""Run a deterministic parser/compiler mutation gate for the RCC frontends."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tests" / "fuzz_corpus.json"
TARGETS = ("i686-unknown-rinos", "x86_64-unknown-rinos")
LANGUAGES = {
    "C17": (".c", "rcc"),
    "C++20": (".cpp", "rcc++"),
}


class FuzzError(ValueError):
    pass


def source_path(spelling: object, language: str) -> Path:
    if not isinstance(spelling, str) or not spelling or "\\" in spelling:
        raise FuzzError("source must be a non-empty POSIX relative path")
    relative = PurePosixPath(spelling)
    if (relative.is_absolute() or
            any(part in ("", ".", "..") for part in relative.parts)):
        raise FuzzError(f"source escapes repository: {spelling}")
    source = ROOT.joinpath(*relative.parts).resolve()
    try:
        source.relative_to(ROOT)
    except ValueError as error:
        raise FuzzError(f"source escapes repository: {spelling}") from error
    if source.suffix != LANGUAGES[language][0] or not source.is_file():
        raise FuzzError(f"source is not a {language} fixture: {spelling}")
    return source


def load_cases() -> list[dict[str, object]]:
    try:
        document = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise FuzzError(f"cannot read manifest: {error}") from error
    if not isinstance(document, dict) or document.get("schema") != 1:
        raise FuzzError("fuzz manifest schema must be 1")
    cases = document.get("cases")
    if not isinstance(cases, list) or not cases:
        raise FuzzError("fuzz manifest must contain cases")
    result: list[dict[str, object]] = []
    seen: set[str] = set()
    for case in cases:
        if not isinstance(case, dict) or set(case) != {"id", "language", "source"}:
            raise FuzzError("fuzz case keys are invalid")
        case_id = case["id"]
        language = case["language"]
        if (not isinstance(case_id, str) or not case_id or case_id in seen or
                not isinstance(language, str) or language not in LANGUAGES):
            raise FuzzError("fuzz case id/language is invalid")
        seen.add(case_id)
        result.append({
            "id": case_id,
            "language": language,
            "source": source_path(case["source"], language),
        })
    return result


def resolve_tool(argument: str | None, default_name: str) -> Path:
    path = Path(argument) if argument else Path("build") / default_name
    if not path.is_absolute():
        path = ROOT / path
    if not path.is_file():
        raise FuzzError(f"compiler not found: {path}")
    return path.resolve()


def valid_mutations(source: str) -> list[tuple[str, str]]:
    return [
        ("baseline", source),
        ("leading-comment", "/* deterministic fuzz */\n" + source),
        ("trailing-comment", source.rstrip() + "\n/* deterministic fuzz */\n"),
        ("blank-lines", source.replace("\n", "\n\n")),
    ]


def invalid_mutations(source: str) -> list[tuple[str, str]]:
    return [
        ("empty-initializer", source + "\nint fuzz_invalid = ;\n"),
        ("invalid-token", source + "\n@fuzz_invalid\n"),
    ]


def digest(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            hasher.update(block)
    return hasher.hexdigest()


def compile_source(tool: Path, language: str, target: str,
                   source: Path, output: Path, timeout: int) -> subprocess.CompletedProcess[str]:
    command = [str(tool), "--target", target]
    if language == "C++20":
        command.append("-std=c++20")
    command.extend(("-O1", "-c", "-o", str(output), str(source)))
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                          timeout=timeout, check=False)


def run_case(case: dict[str, object], tools: dict[str, Path], timeout: int,
             temporary: Path) -> tuple[int, int]:
    case_id = str(case["id"])
    language = str(case["language"])
    source = case["source"]
    assert isinstance(source, Path)
    valid_count = 0
    invalid_count = 0
    for mutation_id, contents in valid_mutations(source.read_text(encoding="utf-8")):
        mutation_dir = temporary / case_id / mutation_id
        mutation_dir.mkdir(parents=True, exist_ok=True)
        mutation = mutation_dir / source.name
        mutation.write_text(contents, encoding="utf-8", newline="\n")
        for target in TARGETS:
            tool = tools[LANGUAGES[language][1]]
            observed: list[str] = []
            for repeat in (1, 2):
                output = mutation_dir / f"{target}-{repeat}.ro"
                completed = compile_source(tool, language, target, mutation,
                                           output, timeout)
                if (completed.returncode != 0 or not output.is_file() or
                        output.stat().st_size == 0):
                    detail = (completed.stdout + completed.stderr).strip()[-2000:]
                    raise FuzzError(
                        f"{case_id}/{mutation_id}/{target}: valid mutation rejected: {detail}")
                observed.append(digest(output))
            if observed[0] != observed[1]:
                raise FuzzError(
                    f"{case_id}/{mutation_id}/{target}: output is nondeterministic")
            valid_count += 1
    for mutation_id, contents in invalid_mutations(source.read_text(encoding="utf-8")):
        mutation_dir = temporary / case_id / mutation_id
        mutation_dir.mkdir(parents=True, exist_ok=True)
        mutation = mutation_dir / source.name
        mutation.write_text(contents, encoding="utf-8", newline="\n")
        for target in TARGETS:
            tool = tools[LANGUAGES[language][1]]
            output = mutation_dir / f"{target}.ro"
            completed = compile_source(tool, language, target, mutation,
                                       output, timeout)
            diagnostic = (completed.stdout + completed.stderr).strip()
            if completed.returncode >= 0 and completed.returncode == 0:
                raise FuzzError(
                    f"{case_id}/{mutation_id}/{target}: invalid mutation accepted")
            if completed.returncode < 0:
                raise FuzzError(
                    f"{case_id}/{mutation_id}/{target}: compiler terminated by signal")
            if not diagnostic or "error:" not in diagnostic:
                raise FuzzError(
                    f"{case_id}/{mutation_id}/{target}: invalid mutation has no error diagnostic")
            if output.is_file() and output.stat().st_size != 0:
                raise FuzzError(
                    f"{case_id}/{mutation_id}/{target}: invalid mutation produced output")
            invalid_count += 1
    print(f"PASS {case_id}: {valid_count} valid and {invalid_count} invalid mutations")
    return valid_count, invalid_count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rcc")
    parser.add_argument("--rccxx")
    parser.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 300:
        print("ERROR: timeout must be in 1..300")
        return 2
    try:
        tools = {
            "rcc": resolve_tool(args.rcc, "rcc"),
            "rcc++": resolve_tool(args.rccxx, "rcc++"),
        }
        cases = load_cases()
        (ROOT / "build").mkdir(parents=True, exist_ok=True)
        valid_count = invalid_count = 0
        with tempfile.TemporaryDirectory(prefix="rcc-fuzz-", dir=ROOT / "build") as directory:
            for case in cases:
                valid, invalid = run_case(case, tools, args.timeout, Path(directory))
                valid_count += valid
                invalid_count += invalid
    except (FuzzError, OSError, subprocess.SubprocessError) as error:
        print(f"ERROR: {error}")
        return 1
    print(f"RCC parser/compiler fuzz gate: {valid_count} valid and {invalid_count} invalid mutations passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
