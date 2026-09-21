"""Check bilingual public Markdown navigation and local links (stdlib only)."""
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]

def slug(text):
    text = re.sub(r'<[^>]+>', '', text).lower()
    return re.sub(r'[^\w\- ]', '', text).replace(' ', '-')

def anchors(text):
    result = set(re.findall(r'<a\s+id=["\']([^"\']+)', text))
    seen = {}
    for title in re.findall(r'^#{1,6}\s+(.+)', text, re.M):
        base = slug(title)
        n = seen.get(base, 0)
        result.add(base + (f'-{n}' if n else ''))
        seen[base] = n + 1
    return result

def main():
    files = sorted(p for folder in (ROOT, ROOT/'docs', ROOT/'bench')
                   for p in (folder.glob('*.md') if folder != ROOT/'docs' else folder.rglob('*.md'))
                   if p.name != 'AGENTS.md' and not p.name.startswith('ESP8266-'))
    errors = []
    for path in files:
        text = path.read_text(encoding='utf-8')
        stem = path.stem.removesuffix('_RU')
        en, ru = stem+'.md', stem+'_RU.md'
        nav = f'[English]({en}) | [Русский]({ru})'
        if not text.startswith(nav):
            errors.append(f'{path.relative_to(ROOT)}: missing language navigation')
        # Code samples can contain illustrative paths that are not documentation links.
        prose = re.sub(r'^```.*?^```\s*$', '', text, flags=re.M|re.S)
        for link in re.findall(r'\]\(([^\s)]+)\)', prose):
            parsed = urlsplit(link.strip('<>'))
            if parsed.scheme or parsed.netloc:
                continue
            target = (path.parent / unquote(parsed.path)).resolve() if parsed.path else path
            if not target.exists():
                errors.append(f'{path.relative_to(ROOT)}: missing {link}')
            elif parsed.fragment and target.suffix == '.md':
                if unquote(parsed.fragment) not in anchors(target.read_text(encoding='utf-8')):
                    errors.append(f'{path.relative_to(ROOT)}: missing anchor {link}')
    for error in errors:
        print(error)
    print(f'Checked {len(files)} Markdown pages; {len(errors)} errors.')
    return bool(errors)

if __name__ == '__main__':
    sys.exit(main())
