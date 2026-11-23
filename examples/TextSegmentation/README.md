# TextSegmentation

This example generates the state machine from
[TextSegmentation_v2.sm](TextSegmentation_v2.sm). It splits UTF-8 input into
atomic spans, then packs them under a SentencePiece Lite token budget.

## Build and run

CMake 3.20, a C++20 compiler and Git are required. Configuration downloads
uni-algo 1.2.0 at a pinned revision; set `FETCHCONTENT_SOURCE_DIR_UNI_ALGO`
to an existing checkout to build without network access. The example uses the
provided `sentence_piece_lite/sentencepiece_lite.{h,cc}` and
`sentence_piece_lite/mt5.spm.fb`. Run from this directory:

```sh
cmake -S ../.. -B build/smc -DBUILD_TESTING=OFF
cmake --build build/smc --target smc_cpp
cmake -S . -B build/demo -DSMC_EXECUTABLE="$PWD/build/smc/smc_cpp"
cmake --build build/demo
ctest --test-dir build/demo --output-on-failure
printf '你好，“这是一句话！里面有逗号，和引号。” Hello world.\n' |
  ./build/demo/text_segmentation sentence_piece_lite/mt5.spm.fb 16 1
```

The CLI accepts `MODEL.spm.fb LIMIT [FIXED_TOKENS]` and reads all input from
standard input. It prints human-readable segment headings, so its stdout is
not a lossless serialization. `FIXED_TOKENS` defaults to zero and reserves
space for fixed engine overhead such as BOS/EOS. Each text segment satisfies
`Encode(segment).size() + FIXED_TOKENS < LIMIT`; printed counts exclude the
overhead. Supply the actual TTS engine's compatible model and account for its
preprocessing and special tokens: the bundled mT5 model is only a demo and
does not establish the budget of an unrelated TTS engine.

## Behavior

- The API returns the original substrings in order: concatenating them exactly
  reconstructs the input, including whitespace and punctuation. Empty input
  returns no segments. Invalid UTF-8 and tokenizer errors are reported.
- Delimiters are newline, CR, `. , ; ? !` and `。 ， ； ？ ！ 、`.
  A delimiter belongs to the preceding atomic span.
- Supported quote pairs are `“”`, `‘’`, `「」`, `『』`, `«»`, ASCII `"`
  and ASCII `'`. Correctly nested quotes form a separate atomic span, which
  may later be packed with adjacent spans. ASCII and curly apostrophes between
  ASCII letters or digits are treated as ordinary text. Ambiguous ASCII quotes
  otherwise close the current matching pair or open a new pair.
- An unmatched closing quote outside a candidate remains ordinary text. On a
  mismatched closer, the candidate through that closer is split using ordinary
  delimiters; at EOF, an unclosed candidate is likewise split. The following
  text is processed normally. Newlines do not terminate a candidate.
- Candidates are re-encoded as complete strings rather than adding token
  counts. Oversized atoms are split at original extended grapheme boundaries,
  preferably after a delimiter or whitespace. Each resulting substring is
  re-encoded, not decoded from token IDs: normalization or unknown tokens can
  make decoding lossy. If no valid partition exists, an error is returned.
  Atomic boundaries around punctuation or quotes themselves are code-point
  boundaries; only the oversized-atom fallback guarantees grapheme boundaries.
- The fast splitting path stops searching after its first failed prefix once it
  has found a fitting one; it does not depend on that failure being monotonic
  for correctness. If its choices strand a suffix, a more expensive exhaustive
  partition search retries without assuming monotonic token counts. Very long
  unbroken spans or pathological tokenizers can therefore be expensive.

`TextSegmentation` accepts a deterministic token-count callback, the strict
limit and optional fixed overhead. The caller must implement the same
preprocessing and token counting as the target engine and keep callback
dependencies alive during `segment()`. The included CLI wraps
`SentencePieceLiteProcessor::Encode` and checks its status.
