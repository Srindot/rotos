"""nasa_check - mechanical checks of the JPL "Power of Ten" rules
(docs/CODING_STANDARD.md) over the ROTOS C code, plus cppcheck.

    python3 tools/nasa_check.py            # report, exit 1 on violations
    python3 tools/nasa_check.py --no-cppcheck

Checks per function / file:
  R1  no goto, setjmp/longjmp; no direct recursion
  R2  no unbounded loops (for(;;), while(1|true)) unless marked
      "JPL-R2 exempt" (the main scheduler loops)
  R3  no malloc / calloc / realloc / free
  R4  functions <= 60 lines
  R5  assertion density (RN_ASSERT per function) reported per directory
  R9  no function pointers outside the vendor HAL (firmware/hal/esp)
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
DIRS = ["core/src", "core/include", "firmware/app", "firmware/hal", "firmware/main",
        "ui/firmware", "sim/host", "sim/screen"]
MAX_LINES = 60
FN_START = re.compile(r"^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\(([^;]*)\)\s*$")
BANNED_CALLS = re.compile(r"\b(malloc|calloc|realloc|free)\s*\(")
JUMPS = re.compile(r"\b(goto|setjmp|longjmp)\b")
FOREVER = re.compile(r"for\s*\(\s*;\s*;\s*\)|while\s*\(\s*(1|true)\s*\)")
FN_POINTER = re.compile(r"\(\s*\*\s*\w+\s*\)\s*\(")
VENDOR_HAL = os.path.join("firmware", "hal", "esp")


def c_files():
    for d in DIRS:
        for base, _, files in os.walk(os.path.join(ROOT, d)):
            for f in sorted(files):
                if f.endswith((".c", ".h")):
                    yield os.path.join(base, f)


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def functions(lines):
    """(name, first line index, last line index) for K&R-style definitions:
    signature line(s), '{' alone in column 0, '}' alone in column 0."""
    out = []
    i = 0
    while i < len(lines):
        if lines[i] == "{" and i > 0:
            j = i - 1
            sig = lines[j]
            while j > 0 and "(" not in sig:
                j -= 1
                sig = lines[j] + " " + sig
            while j > 0 and sig.count("(") > sig.count(")") + 0 and not FN_START.match(sig):
                j -= 1
                sig = lines[j] + " " + sig
            m = re.search(r"\b([A-Za-z_]\w*)\s*\(", sig)
            end = next((k for k in range(i + 1, len(lines)) if lines[k] == "}"), len(lines) - 1)
            if m:
                out.append((m.group(1), j, end))
            i = end + 1
            continue
        i += 1
    return out


def check_file(path, report, stats):
    rel = os.path.relpath(path, ROOT)
    raw = open(path, encoding="utf-8").read()
    code = strip_comments(raw)
    raw_lines, lines = raw.split("\n"), code.split("\n")
    for n, line in enumerate(lines, 1):
        if JUMPS.search(line):
            report.append(f"{rel}:{n}: R1 goto/setjmp")
        if BANNED_CALLS.search(line):
            report.append(f"{rel}:{n}: R3 dynamic allocation")
        if FOREVER.search(line) and "JPL-R2 exempt" not in raw_lines[n - 1]:
            report.append(f"{rel}:{n}: R2 unbounded loop without 'JPL-R2 exempt'")
        if FN_POINTER.search(line) and VENDOR_HAL not in rel:
            report.append(f"{rel}:{n}: R9 function pointer")
    key = os.path.dirname(rel)
    for name, a, b in functions(lines):
        body = "\n".join(lines[a + 1:b])
        length = b - a + 1
        if length > MAX_LINES:
            report.append(f"{rel}:{a + 1}: R4 {name}() is {length} lines (> {MAX_LINES})")
        if re.search(r"\b" + re.escape(name) + r"\s*\(", body):
            report.append(f"{rel}:{a + 1}: R1 {name}() calls itself")
        s = stats.setdefault(key, [0, 0])
        s[0] += 1
        s[1] += len(re.findall(r"\bRN_ASSERT\s*\(", body))


def run_cppcheck():
    exe = shutil.which("cppcheck")
    if exe is None:
        print("cppcheck not installed: skipped")
        return 0
    inc = []
    for d in ("core/include", "core/src/ffi", "firmware/app/include", "ui/firmware/include", "firmware/hal/include"):
        inc += ["-I", os.path.join(ROOT, d)]
    srcs = [os.path.join(ROOT, d) for d in ("core/src", "firmware/app/src", "ui/firmware/src", "sim/host", "sim/screen")]
    cmd = [exe, "--enable=warning,performance,portability", "--std=c11", "--quiet", "--inline-suppr",
           "--error-exitcode=3", "--suppress=missingIncludeSystem"] + inc + srcs
    r = subprocess.run(cmd, capture_output=True, text=True)
    out = (r.stdout + r.stderr).strip()
    print("cppcheck:", "clean" if r.returncode == 0 and not out else "\n" + out)
    return 0 if r.returncode == 0 else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-cppcheck", action="store_true")
    a = ap.parse_args()
    report, stats = [], {}
    files = list(c_files())
    for f in files:
        check_file(f, report, stats)
    print(f"checked {len(files)} files")
    for key in sorted(stats):
        n, asserts = stats[key]
        print(f"  {key:<28} {n:4d} functions, {asserts / max(n, 1):.1f} assertions/function")
    for r in report:
        print(r)
    bad = len(report) > 0
    if not a.no_cppcheck:
        bad = run_cppcheck() != 0 or bad
    print("NASA check:", "FAILED" if bad else "passed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
