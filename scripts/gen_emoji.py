#!/usr/bin/env python3
"""Generates resources/emoji/*.tsv from the Unicode emoji list and CLDR annotations.

Usage: gen_emoji.py <emoji-test.txt> <cldr-dir>
  emoji-test.txt: https://unicode.org/Public/emoji/latest/emoji-test.txt
  cldr-dir: holds ann-<lang>.json and der-<lang>.json from cldr-json
            (cldr-annotations-full/annotations and cldr-annotations-derived-full/annotationsDerived)

Outputs (UTF-8):
  emoji.tsv        "@<group>" lines, then: emoji \\t version \\t English name \\t 5 skin-tone variants or ""
  names_<lang>.tsv emoji \\t "name|keyword|keyword..." (localized search terms)
"""
import json
import re
import sys
from pathlib import Path

TONES = ["light skin tone", "medium-light skin tone", "medium skin tone", "medium-dark skin tone", "dark skin tone"]
LANGS = {"pt_BR": "pt", "es": "es", "en": "en"}
LINE = re.compile(r"^([0-9A-F ]+?)\s*;\s*fully-qualified\s*#\s*(\S+)\s+E(\d+\.\d+)\s+(.+)$")


def parse(path):
    groups = []  # [(group, [entry])]
    by_name = {}
    current = None
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        if raw.startswith("# group:"):
            name = raw.split(":", 1)[1].strip()
            current = None if name == "Component" else (name, [])
            if current:
                groups.append(current)
            continue
        m = LINE.match(raw)
        if not m or current is None:
            continue
        _, emoji, version, name = m.groups()
        if ":" in name:
            base, tone = (s.strip() for s in name.split(":", 1))
            if tone in TONES:
                if base in by_name:
                    by_name[base]["tones"][TONES.index(tone)] = emoji
                continue
            if "skin tone" in tone:
                continue  # mixed tones (e.g. couples with two different tones)
        entry = {"emoji": emoji, "version": version, "name": name, "tones": [""] * 5}
        by_name[name] = entry
        current[1].append(entry)
    return groups


def annotations(cldr, lang):
    table = {}
    for kind in ("ann", "der"):
        data = json.loads((Path(cldr) / f"{kind}-{lang}.json").read_text(encoding="utf-8"))
        root = data.get("annotations") or data.get("annotationsDerived")
        for key, value in root["annotations"].items():
            terms = value.get("tts", []) + value.get("default", [])
            if terms:
                table[key] = terms
    return table


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    groups = parse(sys.argv[1])
    out = Path(__file__).resolve().parent.parent / "resources" / "emoji"
    out.mkdir(parents=True, exist_ok=True)

    lines = []
    for group, entries in groups:
        lines.append(f"@{group}")
        for e in entries:
            tones = " ".join(e["tones"]) if all(e["tones"]) else ""
            lines.append(f"{e['emoji']}\t{e['version']}\t{e['name']}\t{tones}")
    (out / "emoji.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")

    for suffix, lang in LANGS.items():
        table = annotations(sys.argv[2], lang)
        rows = []
        for _, entries in groups:
            for e in entries:
                terms = table.get(e["emoji"]) or table.get(e["emoji"].replace("️", ""))
                if terms:
                    unique = list(dict.fromkeys(t.replace("|", " ").strip() for t in terms))
                    rows.append(f"{e['emoji']}\t{'|'.join(unique)}")
        (out / f"names_{suffix}.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")

    total = sum(len(e) for _, e in groups)
    toned = sum(1 for _, es in groups for e in es if all(e["tones"]))
    print(f"{total} emojis in {len(groups)} groups, {toned} with skin tones")


if __name__ == "__main__":
    main()
