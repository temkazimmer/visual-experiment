# Menu lean column — dimensions

Spawned at Play by `AVisexMenuDirector::SpawnLeanColumn` (not a level actor). Read it at runtime:
`UGameplayStatics::GetAllActorsWithTag(World, "LeanColumn")`, or `MenuDirector->GetLeanColumn()`.

| | Value |
|---|---|
| Actor | tag `LeanColumn`, scale 1 (uniform), no roll |
| Pivot | top of the stone at the middle of its length |
| Axes | X along the drum, Z up |
| Collision | the stone itself (`Stone`, SM_LeanColumn_HD, complex as simple): blocks Visibility and Camera; Pawn only while she walks off |
| Visible stone | `Stone`: SM_LeanColumn_HD (Tools/lean_column_geo.py), 20 flutes, 234k tris, scaled uniformly by the fit |
| Surface radius | **47.2 cm** (median of 63 complex traces over the top; `LeanColumnSurfaceRadius` on the director) |
| Length | 320 cm |
| Axis height | 108.3 cm world (her feet + `LeanColumnRadius` 45 − 3 cm settle) |
| Top height | **155.4 cm** world (pivot Z) |
| Pivot (world) | 9575.8, -848.9, 155.4 |
| Her stand point | 9500, -900 (StandIn), facing yaw -105 |
| Gap | `LeanColumnGap` 24 cm, feet to the nominal 45 cm surface → **21.8 cm** to the measured surface |
| Shift | `LeanColumnShift` 60 cm along the drum to her left |

The director owns these numbers (`Menu|Lean` properties). Main tells movement if any change.

## Body fit (2026-11-11)

`FitLeanColumn()` (every ShowMenu; BlueprintCallable) re-sizes and re-places the drum for her
body. The seat the lean clip was posed on (`LeanSeat`: pelvis 85.22 cm up, 31.5 cm behind her
root, 8.3 cm off the stone, 46 deg from the top, in her mesh space at scale 0.935) scales with
her Height; the drum is solved to meet it at that angle with its underside `LeanColumnSettle`
5.2 cm in the sand. Leg length doesn't move her pelvis or hands in the lean, so it doesn't
move the drum. The numbers in the table above are the default body (Height 166).
Measured in PIE (pelvis bone / hand bones to the stone, cm): Height 145: 7.7 / 3.4, 2.2;
166: 8.9 / 4.0, 2.6; 185: 10.2 / 5.0, 3.4 (was: 145 sank her pelvis 3.2 cm into the stone,
185 floated it 4.2 cm). Legs 0.90 and 1.12: unchanged from 166. Arm length 1.08 brings the
hand bones to 0.1-2 cm (the anim side's hand contact pass owns that).
