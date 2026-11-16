# Fork notes

This project is a modified version of [OpenTTD](https://github.com/OpenTTD/OpenTTD), licensed under
GNU GPL v2 (see `COPYING.md`). All original copyright notices are kept. Modified source code is published under
the same license. This fork is not affiliated with the OpenTTD project.

Transport Tycoon Deluxe graphics/sound/music are NOT included (use OpenGFX / OpenSFX / OpenMSX).

## Changes
- Arabic glyphs merged into the bundled fonts `media/baseset/OpenTTD-{Sans,Small,Serif,Mono}.ttf`.

## Roadmap
UI, companies/finance, vehicles/transport, cargo/industries.

## Note for whoever continues the economy work

There are two additional companies that must be part of the plan (هناك شركتين اضافيتين يجب عليك ان تدخلهن ضمن خطتك):
**SEAMAR** (ships) and **AIRIX** (aircraft). They are dealers of the used vehicle market like TTD MAX (trains),
TAJCAR (road vehicles) and LIMANO (every kind): used offers, installment leases and rentals all work the same way for them.
Dealer numbers are 0 TTD MAX, 1 TAJCAR, 2 LIMANO, 3 SEAMAR, 4 AIRIX. `FORK_USED_SLOTS` stays 24, because it is part of the
savegame layout; the dealers share those slots (6, 6, 6, 3 and 3 offers).
