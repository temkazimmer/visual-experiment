# Menu lean column — dimensions

Spawned at Play by `AVisexMenuDirector::SpawnLeanColumn` (not a level actor). Read it at runtime:
`UGameplayStatics::GetAllActorsWithTag(World, "LeanColumn")`, or `MenuDirector->GetLeanColumn()`.

| | Value |
|---|---|
| Actor | tag `LeanColumn`, scale 1 (uniform), no roll |
| Pivot | top of the stone at the middle of its length |
| Axes | X along the drum, Z up |
| Collision | capsule `Collision`, radius = measured surface radius, half-height 160 (rounded ends), axis along X |
| Visible stone | `Stone`: SM_Drum_0, no blocking collision |
| Surface radius | **47.2 cm** (median of 63 complex traces over the top; `LeanColumnSurfaceRadius` on the director) |
| Length | 320 cm |
| Axis height | 108.3 cm world (her feet + `LeanColumnRadius` 45 − 3 cm settle) |
| Top height | **155.4 cm** world (pivot Z) |
| Pivot (world) | 9575.8, -848.9, 155.4 |
| Her stand point | 9500, -900 (StandIn), facing yaw -105 |
| Gap | `LeanColumnGap` 24 cm, feet to the nominal 45 cm surface → **21.8 cm** to the measured surface |
| Shift | `LeanColumnShift` 60 cm along the drum to her left |

The director owns these numbers (`Menu|Lean` properties). Main tells movement if any change.
