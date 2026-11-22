#!/usr/bin/env python3
"""Library presets -> built-in presets (Presets.h entries).
usage: promote_library.py <library folder> [defaults.txt]
Every .rvpreset may carry these attributes on <RVPRESET>: type (Room/Hall/Chamber/Plate/Spring/Ambient/Non-linear / FX), era (1970s..2010s), use (Vocal/Drums/...),
note (Japanese description), note_en (English description). Writes entries.txt (paste into presets() before the closing of the list) and cats.txt
(paste into presetCats()). Parameters equal to the default are left out; mix is set to 35 like the other fitted presets."""
import re, sys, glob, os
from xml.etree import ElementTree as ET
lib = sys.argv[1]
defp = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), 'defaults.txt')
D = {a: float(b) for a, b in re.findall(r'(\w+)=(\S+)', open(defp, encoding='utf-8').read())} if os.path.exists(defp) else {}
def fmt(k, v):
    if abs(v - round(v)) < 1e-6 and abs(v) < 1e5: return '{ "%s", %d }' % (k, round(v))
    s = '%.4g' % v
    if 'e' in s: s = '%.1f' % v
    if '.' not in s: s += '.0'
    return '{ "%s", %sf }' % (k, s)
ent, cats = [], []
for f in sorted(glob.glob(os.path.join(lib, '**', '*.rvpreset'), recursive=True)):
    r = ET.parse(f).getroot(); name = r.get('name') or os.path.splitext(os.path.basename(f))[0]
    kv = [(p.get('id'), float(p.get('v'))) for p in r.findall('P') if p.get('id') != 'autoLvl']
    d = {k: v for k, v in kv if k not in D or abs(D[k] - v) > 1e-4 * max(1, abs(v))}; d['mix'] = 35.0; d.setdefault('src', 1.0)
    ja = (r.get('note') or name).replace('"', '\\"'); en = (r.get('note_en') or name).replace('"', '\\"')
    ent.append('        { "%s", 3, "%s",\n          "%s",\n          { %s } },' % (name.replace('"', '\\"'), ja, en, ', '.join(fmt(k, v) for k, v in d.items())))
    cats.append('{ "%s", "%s", %s, "%s" }' % (name, r.get('type', 'Room'), ('"%s"' % r.get('era')) if r.get('era') else 'nullptr', r.get('use', 'Vocal')))
open('entries.txt', 'w', encoding='utf-8').write('\n'.join(ent) + '\n')
open('cats.txt', 'w', encoding='utf-8').write('        ' + ', '.join(cats) + ',\n')
print(len(ent), 'presets -> entries.txt, cats.txt')
