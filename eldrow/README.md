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
A second, independently written solver agrees, with and without the pruning bound
(see [Checks](#checks)).

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
from K = 2 (times are cumulative; `logs/search_log.txt`):

| K  | result     | expansions | time   |
|----|------------|-----------:|-------:|
| 15 | found      |      5,634 |  0.7 s |
| 16 | found      |    702,671 |   47 s |
| 17 | impossible |  1,802,709 |  176 s |

## Checks

### The 16-word chain

- `check` (C++) and `verify.py` (Python, written separately from the puzzle text) both
  confirm the chain. `verify.py` also reproduces the puzzle's examples: ERROR/ROVER,
  and TRACK/TIGER with DIRTY, TARDY and TEPID rejected and TOWER allowed. `check` rejects
  the example TRACK,TROMP,TIGER because TROMP is not in the word list.
- A second 16-word chain, found by `exhaust`, also passes:
  `GAZER,INNER,QUEER,ODDER,BOXER,FOYER,HOVER,JOKER,WOOER,COWER,LOWER,MOWER,POWER,ROWER,SOWER,TOWER`.

### That no 17-word chain exists

Each step of the argument was checked separately:

1. **Word list.** `pdftotext -layout` and `pdftotext -raw` give the same 2,315 words. The
   PDF (a Google Sheets export with no images) is one column: 53 pages of 43 words plus 36
   on the last page, from ABACK to ZONAL.
2. **Feedback.** `fbmatrix.py` builds all 2,315 × 2,315 colourings with `verify.py`'s
   `mark()`. They match the C++ `feedback()` byte for byte on all 5,359,225 pairs.
3. **Treating the candidate set as the whole state.** `rulecheck.py` takes 60 small
   sub-lists, chosen to include long families like ?IGHT, ?OWER and ?ATCH. For every target
   it lists every guess sequence, checking each guess against the puzzle's rule directly.
   Its maxima match `search` and `exhaust` on all 60.
4. **An independent solver.** `exhaust.cpp` shares no code with `search.cpp`. It reads
   the Python feedback matrix and keys its memo on the exact member list of each state
   (no hashing). It tries every legal guess and every feedback class at every state.
   - With the letter bound: no 17-word chain (1.9 M expansions, 67 s on 4 threads,
     `logs/exhaust_k17_letters.txt`).
   - **With no bound except the trivial `f(S) <= |S|`**: no 17-word chain (214.6 M
     expansions, 21 min on 4 threads, `logs/exhaust_k17_none.txt`). This run does not use
     the letter-bound argument at all.
5. `crosscheck.py` compares `search` with a plain exhaustive search on 40 random sub-lists
   of 15–45 words. All 40 optima agree.

## Reproduce

```sh
cd eldrow
make
./search words.txt 2 26          # about 3 minutes, ends with "K=17 IMPOSSIBLE"
./check words.txt "QUEUE,GAZER,INNER,ODDER,FOYER,BOXER,JOKER,HOVER,WOOER,COWER,LOWER,MOWER,POWER,ROWER,SOWER,TOWER"
python3 verify.py words.txt "QUEUE,GAZER,INNER,ODDER,FOYER,BOXER,JOKER,HOVER,WOOER,COWER,LOWER,MOWER,POWER,ROWER,SOWER,TOWER"
python3 crosscheck.py ./search
make fb.bin                      # feedback matrix from verify.py, for exhaust
./exhaust words.txt fb.bin 17 letters 4   # about 1 minute: "NO chain of 17 words exists"
./exhaust words.txt fb.bin 17 none 4      # about 21 minutes, no pruning bound
python3 rulecheck.py ./search ./exhaust
```
