# Ember Action Foundation Fixtures

These synthetic fixtures exercise the isolated SKIN2-002 foundation. They do
not name production commands or states and do not activate or execute actions.

| Fixture | Expected result |
| --- | --- |
| `positive/canonical-visual.json` | Valid; canonical source and digest equal the expert-source fixture. |
| `positive/canonical-text.json` | Valid; authoring source, key order, escape spelling, number spelling, and node metadata do not change runtime source identity. |
| `negative/missing-node-reference.json` | `EA_GRAPH_MISSING_REFERENCE` |
| `negative/graph-cycle.json` | `EA_GRAPH_CYCLE` |
| `negative/hash-mismatch.json` | `EA_HASH_MISMATCH` |

The focused native test also covers duplicate JSON keys, source byte/depth
limits, immutable registry misses, and action-declared node ceilings from
bounded in-memory mutations. The broader issue #65 matrix remains deferred
until the generated #64/#31 registry contracts are available.

## Canonical source vector

Both positive fixtures normalize to the same runtime source and digest:

```text
sha256:d4a1c640aef1fba1ca061ed0e3abe4c2f106d46adc272f5eb09bff0001e3b1c5
```

Canonical source sorts object keys by unsigned UTF-8 bytes, preserves array
order, uses shortest finite round-trip number spelling, normalizes negative
zero to zero and CR/CRLF string content to LF, and erases JSON escape spelling.
Root `source`, supplied `contentHash`, and per-node `metadata` are authoring or
verification data and do not contribute to runtime source identity. The
compiled cache key must additionally include registry and compiler generation;
that later cache is intentionally not implemented in this safe-parallel slice.
