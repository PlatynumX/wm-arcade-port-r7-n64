# N64-first architecture

The gameplay engine is plain C. Libdragon is an adapter around it, not the engine itself.

r6 uses RDPQ for CI8/TLUT wrestler art and draws two independent fighters in one frame. The host build is a deterministic verification harness for state transitions, combat contact, bounds and AI. Stock 4 MB N64 RAM remains the minimum target; source artwork should be converted/streamed intentionally rather than assuming Expansion Pak memory.
