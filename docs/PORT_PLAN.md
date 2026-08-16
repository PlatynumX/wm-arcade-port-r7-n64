# Port plan

## Architectural rule

N64 is the primary runtime target, but gameplay/source translation remains portable C. Platform-specific rendering, controller and device work stays under `src/platform/`.

## Completed path

- r1: portable scheduler / fixed-point / animation-VM scaffold
- r2: real source-derived FINISEQ command stream
- r3: source animation timing driving visible N64 proxy
- r4: real Bret WIMP artwork on real N64
- r5: RDP CI8/TLUT rendering, four-way movement, run, punch/kick visual slices
- r6: two-fighter combat sandbox, CPU opponent, health/damage/stun/knockback, depth sorting

## Next source-accuracy milestones

1. Translate Midway attack command/hitbox semantics instead of r6 visual contact windows.
2. Translate hit reactions and recovery animations.
3. Generalize wrestler source generation and add a second distinct arcade wrestler.
4. Rope collision/rebound semantics from the original source.
5. Grapple state machine.
6. Sound command compatibility layer and N64 audio backend.
7. Original ring/background artwork and match UI.
