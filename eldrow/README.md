# Eldrow: the longest hard-mode Wordle chain

## Answer (16 words, proven optimal)

```
QUEUE,GAZER,INNER,ODDER,FOYER,BOXER,JOKER,HOVER,WOOER,COWER,LOWER,MOWER,POWER,ROWER,SOWER,TOWER
```

The target is **TOWER**. Colours against TOWER (`G` green, `Y` yellow, `-` gray):

```
QUEUE  --Y--      WOOER  YG-GG
GAZER  ---GG      COWER  -GGGG
INNER  ---GG      LOWER  -GGGG
ODDER  Y--GG      MOWER  -GGGG
FOYER  -G-GG      POWER  -GGGG
BOXER  -G-GG      ROWER  -GGGG
JOKER  -G-GG      SOWER  -GGGG
HOVER  -G-GG      TOWER  GGGGG
```

Each word is a possible mystery word given the colours of every guess before it. An exhaustive
search shows that **no chain of 17 words exists**, so 16 is the maximum for this word list.

## Rules as implemented

- `words.txt` holds the 2,315 words from the puzzle's PDF (the original Wordle answer list).
- Feedback marks greens first, then yellows from left to right, counting each letter of the
  mystery word once. ERROR against ROVER gives yellow, yellow, gray, yellow, green.
- Hard mode uses the puzzle's own definition: guess *i* must be a word that could be the
  mystery word, i.e. `feedback(g_j, g_i) == feedback(g_j, target)` for every earlier guess
  `g_j`. The last word is the target.

## How the search works (`search.cpp`)

A state is the set `S` of words still consistent with every clue. Guessing `w` in `S` splits
`S \ {w}` into feedback classes, and the next state is the class the target is in. So the
longest chain from `S` (counting the final guess) is

    f(S) = max(1, 1 + max over w in S and feedback classes P of f(P))

and the answer is `f(all 2,315 words)`. The search asks "is `f(S) >= K`?" for K = 2, 3, ...
depth-first, with a memo keyed on a 128-bit hash of `S`. It prunes with this bound:

> In a chain `w_1 .. w_m = T` inside `S`, the last wrong guess `w_{m-1}` is non-green at some
> position `p`. A letter that turns green at `p` stays there for the rest of the chain, and a
> letter that is not green at `p` can never be played at `p` again. So `w_1 .. w_{m-1}` have
> different letters at `p`, none of them `T[p]`, and `m <= L_p(S)`, the number of distinct
> letters in position `p` across `S`. So `f(S) <= min(|S|, max_p L_p(S))`.

For the full list this bound is 26, the puzzle's upper bound. Results on one core, run
from K = 2 (times are cumulative; `search_log.txt`):

| K  | result     | expansions | time   |
|----|------------|-----------:|-------:|
| 15 | found      |      5,634 |  0.7 s |
| 16 | found      |    702,671 |   47 s |
| 17 | impossible |  1,802,709 |  176 s |

## Checks

- `check` (C++) and `verify.py` (Python, written separately from the puzzle text) both
  confirm the 16-word chain. `verify.py` also reproduces the puzzle's examples: ERROR/ROVER,
  and TRACK/TIGER with DIRTY, TARDY and TEPID rejected and TOWER allowed. TROMP is rejected
  because it is not in the list.
- `crosscheck.py` compares the pruned search with a plain exhaustive search (no bound) on 40
  random sub-lists of 15–45 words. All 40 optima agree.

## Reproduce

```sh
cd eldrow
make
./search words.txt 2 26          # about 3 minutes, ends with "K=17 IMPOSSIBLE"
./check words.txt "QUEUE,GAZER,INNER,ODDER,FOYER,BOXER,JOKER,HOVER,WOOER,COWER,LOWER,MOWER,POWER,ROWER,SOWER,TOWER"
python3 verify.py words.txt "QUEUE,GAZER,INNER,ODDER,FOYER,BOXER,JOKER,HOVER,WOOER,COWER,LOWER,MOWER,POWER,ROWER,SOWER,TOWER"
python3 crosscheck.py ./search
```
