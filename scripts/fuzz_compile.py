#!/usr/bin/env python3
"""Run a deterministic parser/compiler mutation gate for the RCC frontends."""

from __future__ import annotations

import argparse
import hashlib
import json
import random
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
GENERATED_MUTATION_COUNT = 6


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


def generated_expression(generator: random.Random, depth: int) -> str:
    leaves = (
        "a", "b", "value", "pair.left", "pair.right",
        "0x13579bdf2468ace0ULL", "0xfedcba9876543210ULL",
    )
    if depth == 0 or generator.randrange(5) == 0:
        return generator.choice(leaves)
    form = generator.randrange(4)
    if form == 0:
        return f"(~({generated_expression(generator, depth - 1)}))"
    if form == 1:
        condition = generated_expression(generator, depth - 1)
        yes = generated_expression(generator, depth - 1)
        no = generated_expression(generator, depth - 1)
        return f"((({condition}) & 1ULL) ? ({yes}) : ({no}))"
    left = generated_expression(generator, depth - 1)
    right = generated_expression(generator, depth - 1)
    operator = generator.choice(("+", "-", "*", "&", "|", "^", "<<", ">>"))
    if operator in ("<<", ">>"):
        right = f"(({right}) & 63ULL)"
    return f"(({left}) {operator} ({right}))"


def grammar_generated_mutations(case_id: str, language: str,
                                source: str) -> list[tuple[str, str]]:
    seed_material = f"{case_id}\0{language}\0{source}".encode("utf-8")
    seed = hashlib.sha256(seed_material).digest()
    generator = random.Random(int.from_bytes(seed[:8], "big"))
    suffix = seed.hex()[:12]
    mutations: list[tuple[str, str]] = []
    for index in range(GENERATED_MUTATION_COUNT):
        tag = f"{suffix}_{index}"
        pair_type = f"rcc_fuzz_pair_{tag}"
        function = f"rcc_fuzz_function_{tag}"
        identity = f"rcc_fuzz_identity_{tag}"
        template = ""
        result = "value"
        pair_initialization = f"struct {pair_type} pair = {{a, b}};"
        if language == "C++20":
            template = (
                f"template <int Bias> unsigned long long {identity}("
                "unsigned long long item) { return item + Bias; }\n"
            )
            result = f"{identity}<1>(value)"
            pair_initialization = f"{pair_type} pair{{a, b}};"
        branch_a = generated_expression(generator, 3)
        branch_b = generated_expression(generator, 3)
        body = (
            f"{template}"
            f"struct {pair_type} {{ unsigned long long left; "
            "unsigned long long right; };\n"
            f"unsigned long long {function}(unsigned long long a, "
            "unsigned long long b) {\n"
            f"    {pair_initialization}\n"
            "    unsigned long long value = pair.left ^ pair.right;\n"
            "    for (unsigned int iteration = 0; iteration < 3U; ++iteration) {\n"
            "        if ((value & 1ULL) != 0ULL) {\n"
            f"            value = {branch_a};\n"
            "        } else {\n"
            f"            value = {branch_b};\n"
            "        }\n"
            "    }\n"
            f"    return {result};\n"
            "}\n"
        )
        mutations.append((f"grammar-{index:03d}", source.rstrip() + "\n" + body))
    return mutations


def digest(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(65536), b""):
            hasher.update(block)
    return hasher.hexdigest()


def compile_source(tool: Path, language: str, target: str,
                   source: Path, output: Path, timeout: int) -> subprocess.CompletedProcess[str]:
    command = [str(tool), "--target", target]
    command.append("-std=c17" if language == "C17" else "-std=c++20")
    command.extend(("-O1", "-c", "-o", str(output), str(source)))
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True,
                          timeout=timeout, check=False)


def run_case(case: dict[str, object], tools: dict[str, Path], timeout: int,
             temporary: Path) -> tuple[int, int, int]:
    case_id = str(case["id"])
    language = str(case["language"])
    source = case["source"]
    assert isinstance(source, Path)
    original = source.read_text(encoding="utf-8")
    valid_inputs = valid_mutations(original)
    generated_inputs = grammar_generated_mutations(case_id, language, original)
    valid_inputs.extend(generated_inputs)
    valid_count = 0
    invalid_count = 0
    for mutation_id, contents in valid_inputs:
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
    for mutation_id, contents in invalid_mutations(original):
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
    print(
        f"PASS {case_id}: {len(generated_inputs)} grammar-generated trees, "
        f"{valid_count} valid source/target pairs, {invalid_count} invalid cases"
    )
    return valid_count, invalid_count, len(generated_inputs)


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
        valid_count = invalid_count = generated_count = 0
        with tempfile.TemporaryDirectory(prefix="rcc-fuzz-", dir=ROOT / "build") as directory:
            for case in cases:
                valid, invalid, generated = run_case(
                    case, tools, args.timeout, Path(directory)
                )
                valid_count += valid
                invalid_count += invalid
                generated_count += generated
    except (FuzzError, OSError, subprocess.SubprocessError) as error:
        print(f"ERROR: {error}")
        return 1
    print(
        "RCC deterministic parser/compiler fuzz gate: "
        f"{generated_count} grammar-generated trees, {valid_count} valid "
        f"source/target pairs, and {invalid_count} invalid cases passed"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
