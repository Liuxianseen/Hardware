from __future__ import annotations

import re
import sys
from pathlib import Path

from pycparser import c_parser


ROOT = Path(__file__).parents[1]


def sanitize(text: str) -> str:
    text = text.replace("\r\n", "\n")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"#ifdef __cplusplus\nextern \"C\" \{\n#endif\n", "", text)
    text = re.sub(r"#ifdef __cplusplus\n\}\n#endif\n", "", text)
    text = re.sub(r"^\s*#.*$", "", text, flags=re.MULTILINE)
    return text


def main() -> int:
    prelude = """
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
typedef unsigned int size_t;
typedef int bool;
typedef int va_list;
"""
    header_order = [
        "tester_types.h",
        "tester_platform.h",
        "pcal6524.h",
        "tester_scan.h",
        "cable_profile.h",
        "cable_analysis.h",
        "tester_report.h",
        "tester_app.h",
    ]
    headers = "\n".join(
        sanitize((ROOT / "include" / name).read_text(encoding="utf-8")) for name in header_order
    )
    parser = c_parser.CParser()
    failures: list[str] = []
    source_paths = [
        *sorted((ROOT / "src").glob("*.c")),
        *sorted((ROOT / "tests").glob("test_*.c")),
    ]

    for source_path in source_paths:
        source = sanitize(source_path.read_text(encoding="utf-8"))
        unit = prelude + headers + "\n" + source
        try:
            parser.parse(unit, filename=str(source_path))
        except Exception as exc:  # pycparser gives coordinates in the synthetic unit.
            failures.append(f"{source_path.name}: {exc}")

    if failures:
        print("C syntax validation failed:")
        print("\n".join(failures))
        return 1
    print(f"C syntax validation passed: {len(source_paths)} translation units")
    return 0


if __name__ == "__main__":
    sys.exit(main())
