"""Lightweight #include tracking using union-of-all-declarations strategy."""

from pathlib import Path


def parse_includes(source: str) -> list[str]:
    """Extract #include directive targets from source text."""
    includes: list[str] = []
    for line in source.splitlines():
        stripped = line.strip()
        if stripped.startswith('#include'):
            # Match both <...> and "..."
            parts = stripped.split(None, 1)
            if len(parts) < 2:
                continue
            target = parts[1].strip()
            if target.startswith('<') and target.endswith('>'):
                includes.append(target[1:-1])
            elif target.startswith('"') and target.endswith('"'):
                includes.append(target[1:-1])
    return includes
