from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        print(f"VERSION is not a semantic version: {version!r}", file=sys.stderr)
        return 1

    tag = f"v{version}"
    declarations = {
        "openstrata.toml": rf'^version = "{re.escape(version)}"$',
        "plugins/raster-geotiff/openstrata.plugin.yaml":
            rf'^  version: {re.escape(version)}$',
        "plugins/raster-geotiff/CMakeLists.txt":
            rf'^    VERSION {re.escape(version)}$',
        "CHANGELOG.md": rf'^## \[{re.escape(version)}\] - \d{{4}}-\d{{2}}-\d{{2}}$',
        f"docs/releases/{tag}.md": rf'^# {re.escape(tag)}$',
        "docs/releases/README.md":
            rf'^\| {re.escape(tag)} \| .*\[{re.escape(tag)}\.md\]\({re.escape(tag)}\.md\)',
    }

    failures = []
    for relative_path, pattern in declarations.items():
        path = ROOT / relative_path
        if not path.is_file() or re.search(
            pattern, path.read_text(encoding="utf-8"), flags=re.MULTILINE
        ) is None:
            failures.append(relative_path)

    if failures:
        print(f"Release metadata does not match VERSION {version}:", file=sys.stderr)
        for relative_path in failures:
            print(f"- {relative_path}", file=sys.stderr)
        return 1

    print(f"Release metadata is consistent: {version}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
