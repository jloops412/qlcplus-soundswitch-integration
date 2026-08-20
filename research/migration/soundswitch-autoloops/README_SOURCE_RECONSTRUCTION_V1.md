# Current-rig Source Reconstruction V1

Status: physical-comparison artifact notes, 2026-08-18.

The source-decoder checkpoint in `2026-08-18-v2.2.2-source-decoder-checkpoint.md` was exercised against the connected `The SoundSwitch Shortcut v2.2.2.ssproj.zip` source to build a bounded Preview-101 comparison bundle outside Git.

Observed generation result:

- 96 exact source Autoloop identities/placements/lengths represented across 25 four-loop format-1 chunk projects;
- 20 source-informed states per 8-bar Autoloop and 24 per 16-bar Autoloop;
- source snap boundaries preferentially retained as `Cut`, other sampled intervals translated to `Linear`;
- one current-rig Static Look project containing the first 16 source looks with decoded current-rig intensity/RGBWAUV intent;
- the remaining 16 source Static Looks retained as `MissingDependency` rather than fabricated because their source targets/functions are outside the current four-IR-4/four-BO-TUBE192 mapping;
- destination patch used for physical comparison: IR-4 U1 001/011/021/031 in 10CH, BO-TUBE192 U1 041/121/201/281 in 80CH;
- generated chunk assignment maxima remained below the current 32,768 assignment ceiling (largest observed chunk: 32,384);
- independent parser-style validation across all 26 generated project files reported zero structural errors.

This exercise confirms the format-1 capacity problem described in the decoder checkpoint: a single Preview-101 project cannot represent all 96 source timelines at useful sampled fidelity without exceeding Look/assignment limits. Do **not** turn the chunking strategy into the production migration architecture. The production path remains the semantic Autoloops V2 source/program model under #57/#60.

The generated files are user test artifacts and are intentionally not committed to Git. Source bytes also remain outside Git.
