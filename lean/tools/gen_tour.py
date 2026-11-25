"""Write a tour file (line 2: "(t, sq), ...") as a Lean `List Move` definition."""
import re, sys
from fractions import Fraction as F

def lean_tour(name, path_or_text, doc):
    txt = path_or_text
    if not txt.lstrip().startswith('('):
        lines = open(path_or_text).read().splitlines()
        txt = lines[1] if len(lines) > 1 and lines[0].strip().isdigit() else lines[0]
    mv = re.findall(r'\(([\d/]+),\s*([a-h])([1-8])\)', txt)
    items = []
    for t, f, r in mv:
        t = F(t)
        ts = str(t.numerator) if t.denominator == 1 else f'({t.numerator}/{t.denominator})'
        items.append(f'⟨{ts}, ({ord(f)-97}, {int(r)-1})⟩')
    body = ',\n  '.join(', '.join(items[i:i+6]) for i in range(0, len(items), 6))
    return f'/-- {doc} -/\ndef {name} : List Move := [\n  {body}]\n'

if __name__ == '__main__':
    print(lean_tour(sys.argv[1], sys.argv[2], sys.argv[3]))
