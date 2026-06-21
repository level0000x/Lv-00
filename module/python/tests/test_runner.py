# -*- coding: utf-8 -*-
"""
Syntax-validation test runner for Lv00 (no C DLL required).

Walks the test directory, checks every ``test_*.py`` file for valid Python
syntax, and optionally executes the test files.

Usage::

    # Syntax check only (default)
    python test_runner.py

    # Syntax check + actually run the tests
    python test_runner.py --run

    # Run tests from a custom directory
    python test_runner.py --test-dir path/to/tests --run

Output is a pass/fail summary printed to stdout.
"""

import ast
import os
import subprocess
import sys
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
TEST_DIR = os.environ.get("LV00_TEST_DIR", THIS_DIR)
FILE_PATTERN = "test_*.py"


# ---------------------------------------------------------------------------
# Core logic
# ---------------------------------------------------------------------------

def check_syntax(filepath: str) -> Tuple[bool, str]:
    """Check whether *filepath* contains valid Python syntax.

    Uses :func:`ast.parse` to perform the check, so no C extensions or
    imports are evaluated.

    Args:
        filepath: Absolute or relative path to a ``.py`` file.

    Returns:
        A tuple ``(ok, message)`` where *ok* is ``True`` when the syntax
        is valid and *message* is either ``"OK"`` or an error description.
    """
    try:
        with open(filepath, "r", encoding="utf-8") as fh:
            source = fh.read()
        ast.parse(source, filename=filepath)
        return True, "OK"
    except SyntaxError as exc:
        return False, f"SyntaxError: {exc}"
    except Exception as exc:
        return False, f"Error reading file: {exc}"


def find_test_files(root: str, pattern: str = FILE_PATTERN) -> List[str]:
    """Recursively find all files matching *pattern* under *root*.

    Args:
        root: Directory to walk.
        pattern: Glob-style filename pattern (simple prefix/suffix match).

    Returns:
        Sorted list of absolute file paths.
    """
    matches: List[str] = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for fname in filenames:
            if fname.startswith("test_") and fname.endswith(".py"):
                matches.append(os.path.join(dirpath, fname))
    return sorted(matches)


def run_test_file(filepath: str) -> Tuple[bool, str]:
    """Execute a single test file in a subprocess.

    Args:
        filepath: Path to the test ``.py`` file.

    Returns:
        ``(ok, message)`` — *ok* is ``True`` if the process exited with
        code 0, and *message* contains the captured stderr (truncated).
    """
    try:
        proc = subprocess.run(
            [sys.executable, filepath],
            capture_output=True,
            text=True,
            timeout=30,
            cwd=os.path.dirname(filepath),
        )
        ok = proc.returncode == 0
        detail = proc.stderr.strip()[:500] if not ok else "OK"
        return ok, detail
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT (>30 s)"
    except Exception as exc:
        return False, str(exc)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _print_header(title: str) -> None:
    print(f"\n{'=' * 60}")
    print(f"  {title}")
    print(f"{'=' * 60}\n")


def main() -> int:
    """Entry point.

    Returns:
        0 on all-clear, 1 if any syntax or runtime failure was detected.
    """
    # Parse simple CLI flags
    do_run = "--run" in sys.argv
    custom_dir: Optional[str] = None
    for i, arg in enumerate(sys.argv):
        if arg == "--test-dir" and i + 1 < len(sys.argv):
            custom_dir = sys.argv[i + 1]

    test_root = custom_dir if custom_dir else TEST_DIR

    if not os.path.isdir(test_root):
        print(f"ERROR: test directory not found: {test_root}", file=sys.stderr)
        return 2

    # -- Phase 1: syntax check ------------------------------------------------
    _print_header("Phase 1 — Syntax Check (ast.parse)")
    files = find_test_files(test_root)

    if not files:
        print(f"No test_*.py files found under {test_root}")
        return 0

    print(f"Found {len(files)} test file(s) in {test_root}\n")

    syntax_results: Dict[str, Tuple[bool, str]] = {}
    for fpath in files:
        rel = os.path.relpath(fpath, test_root)
        ok, msg = check_syntax(fpath)
        syntax_results[fpath] = (ok, msg)
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {rel}")
        if not ok:
            print(f"         {msg}")

    syntax_pass = sum(1 for ok, _ in syntax_results.values() if ok)
    syntax_total = len(syntax_results)
    print(f"\n  Syntax summary: {syntax_pass}/{syntax_total} passed")

    # -- Phase 2: runtime (optional) ------------------------------------------
    run_results: Dict[str, Tuple[bool, str]] = {}
    if do_run:
        _print_header("Phase 2 — Runtime Execution")
        for fpath in files:
            rel = os.path.relpath(fpath, test_root)
            ok, msg = run_test_file(fpath)
            run_results[fpath] = (ok, msg)
            status = "PASS" if ok else "FAIL"
            print(f"  [{status}] {rel}")
            if not ok:
                # Show first few lines of the error
                lines = msg.splitlines()
                for line in lines[:4]:
                    print(f"         {line}")
        run_pass = sum(1 for ok, _ in run_results.values() if ok)
        run_total = len(run_results)
        print(f"\n  Runtime summary: {run_pass}/{run_total} passed")

    # -- Overall --------------------------------------------------------------
    _print_header("Overall")
    all_syntax_ok = all(ok for ok, _ in syntax_results.values())
    all_run_ok = True
    if do_run:
        all_run_ok = all(ok for ok, _ in run_results.values())

    if all_syntax_ok and all_run_ok:
        print("  ALL CHECKS PASSED")
        return 0
    else:
        if not all_syntax_ok:
            print("  Some syntax checks FAILED")
        if not all_run_ok:
            print("  Some runtime tests FAILED")
        return 1


if __name__ == "__main__":
    sys.exit(main())
