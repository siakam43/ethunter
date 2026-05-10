#!/usr/bin/env python
"""Scan all et_bench examples and generate a JSON manifest for LLM review agents."""
import json
from pathlib import Path

ET_BENCH_DIR = Path(__file__).parent

def discover_examples():
    """Return list of (category, example_name, fixture_c, ground_truth_json) tuples."""
    examples = []
    for entry in sorted(ET_BENCH_DIR.iterdir()):
        if not entry.is_dir() or entry.name.startswith('.'):
            continue
        category = entry.name
        for ex in sorted(entry.iterdir()):
            if not ex.is_dir():
                continue
            fixture = ex / 'fixture.c'
            gt = ex / 'ground_truth.json'
            if fixture.exists() and gt.exists():
                examples.append((category, ex.name, fixture, gt))
    return examples

def build_manifest():
    examples = discover_examples()
    manifest = {
        "total": len(examples),
        "categories": {},
        "examples": []
    }

    for category, name, fixture, gt in examples:
        gt_data = json.loads(gt.read_text())
        gt_list = gt_data.get("examples", [])
        if category not in manifest["categories"]:
            manifest["categories"][category] = 0
        manifest["categories"][category] += 1

        manifest["examples"].append({
            "category": category,
            "name": name,
            "fixture": str(fixture.relative_to(ET_BENCH_DIR.parent.parent.parent)),
            "ground_truth": gt_data,
            "claims": gt_list
        })

    out = ET_BENCH_DIR / "verification_manifest.json"
    out.write_text(json.dumps(manifest, indent=2))
    print(f"Manifest written: {out}")
    print(f"Total examples: {manifest['total']}")
    for cat, count in sorted(manifest['categories'].items()):
        print(f"  {cat}: {count}")

if __name__ == "__main__":
    build_manifest()
