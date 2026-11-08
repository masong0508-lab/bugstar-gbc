#!/usr/bin/env python3
"""Finds literal texts that do not fit: txt_line/put_line/txt_put with a string literal that runs past its width or past the 20-tile screen."""
import re, glob
bad = 0
for f in sorted(glob.glob('src/*.c')):
    for n, l in enumerate(open(f, errors='replace'), 1):
        for m in re.finditer(r'(txt_line|put_line)\(\s*(\d+)\s*,\s*[^,]+,\s*"((?:[^"\\]|\\.)*)"\s*,\s*(\d+)\s*\)', l):
            x, s, w = int(m.group(2)), m.group(3), int(m.group(4)); L = len(re.sub(r'\\[0-7]{1,3}|\\.', 'x', s))
            if L > w or x + w > 20: print('%s:%d: "%s" len %d in width %d at x %d' % (f, n, s, L, w, x)); bad += 1
        for m in re.finditer(r'txt_put\(\s*(\d+)\s*,\s*[^,]+,\s*"((?:[^"\\]|\\.)*)"\s*\)', l):
            x, s = int(m.group(1)), m.group(2); L = len(re.sub(r'\\[0-7]{1,3}|\\.', 'x', s))
            if x + L > 20: print('%s:%d: txt_put "%s" runs to column %d' % (f, n, s, x + L)); bad += 1
print(bad, 'problems')
