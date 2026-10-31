# Step 11: boil a sprite picture down 2x2 -> 1 (shared by the generators). Pictures are rows of ".123" (0 clear, 1 dark, 2 body, 3 light).
def halve(rows):
    h, w = len(rows) // 2, len(rows[0]) // 2
    g = [[0] * w for _ in range(h)]
    lone = []
    for y in range(h):
        for x in range(w):
            blk = [".123".index(rows[2 * y + dy][2 * x + dx]) for dy in (0, 1) for dx in (0, 1)]
            op = [v for v in blk if v]
            if len(op) >= 2: g[y][x] = 3 if 3 in op else max(sorted(set(op)), key=op.count)
            elif len(op) == 1: lone.append((y, x, op[0]))
    for y, x, v in lone:                                   # a lone pixel (a thin leg, a wing tip) stays if nothing is next to it
        nb = [g[yy][xx] for yy, xx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)) if 0 <= yy < h and 0 <= xx < w]
        if not any(nb): g[y][x] = v
    return ["".join(".123"[v] for v in r) for r in g]
