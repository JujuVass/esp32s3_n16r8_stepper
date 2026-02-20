#!/usr/bin/env python3
"""
Find functions that are likely never called.
Counts how many times each function name appears across all source files.
- Count 1: only the definition exists (DEAD for sure)
- Count 2: declaration + definition, but no call (DEAD)
- Count 3: might have exactly 1 call site (CHECK manually)
"""

import re
import sys
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
root = root.resolve()
exts = {".h", ".cpp", ".ino", ".c"}
skip = {".pio", ".git", "node_modules", "lib", "test", "data"}

# Collect source files
contents = {}
for d in ["src", "include"]:
    dir_path = root / d
    if not dir_path.exists():
        continue
    for f in dir_path.rglob("*"):
        if f.suffix in exts and not any(s in f.parts for s in skip):
            contents[str(f)] = f.read_text(encoding="utf-8", errors="replace")

# Strip comments and string literals
def clean(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
    return text

all_clean = {k: clean(v) for k, v in contents.items()}
all_text = "\n".join(all_clean.values())

# Regex for function definitions (with body { )
func_def_re = re.compile(
    r"^\s*(?:static\s+)?(?:inline\s+)?(?:virtual\s+)?(?:const\s+)?"
    r"(?:[\w:<>*&]+\s+)+?"
    r"((?:\w+::)?(\w+))"
    r"\s*\([^;]*\)\s*(?:const\s*)?(?:override\s*)?\{",
    re.MULTILINE,
)

# Framework entry points and noise words to skip
FRAMEWORK = {
    "setup", "loop", "motorTask", "networkTask", "sendStatus", "stopMovement",
    "setRgbLed", "logStackHighWaterMark", "logDebugDiagnostics", "main",
}
NOISE = {
    "if", "for", "while", "else", "return", "switch", "case", "do",
    "new", "delete", "sizeof", "void", "int", "bool", "float", "char",
    "String", "get", "set", "begin", "end", "read", "write", "init",
    "start", "stop", "process", "send", "print", "println",
}

results = []
seen = set()  # avoid duplicate entries for same base name

for fpath, content in all_clean.items():
    for m in func_def_re.finditer(content):
        full_name = m.group(1)
        base = m.group(2)
        if base in FRAMEWORK or base in NOISE or len(base) < 4:
            continue
        if base.startswith("_") or base.startswith("operator"):
            continue
        if base in seen:
            continue
        seen.add(base)

        # Count total word-boundary occurrences across ALL files
        count = len(re.findall(r"\b" + re.escape(base) + r"\b", all_text))
        rel = str(Path(fpath).relative_to(root))
        line = content[: m.start()].count("\n") + 1
        results.append((count, base, full_name, rel, line))

# Sort by count ascending
results.sort()

print(f"Count | Function | File | Tag")
print("-" * 100)
dead_count = 0
check_count = 0
for count, base, full, fpath, line in results:
    if count <= 2:
        dead_count += 1
        print(f"  {count}   | {full:<45} | {fpath}:{line} | DEAD")
    elif count == 3:
        check_count += 1
        print(f"  {count}   | {full:<45} | {fpath}:{line} | CHECK")

print(f"\nTotal: {dead_count} likely dead (count<=2), {check_count} to verify (count=3)")
