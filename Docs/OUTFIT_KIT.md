# Outfit Kit

One way to add outfits from now on: Tema draws the outfit on her body in Blender, the
drawing becomes exact body coordinates, and agents build from those instead of guessing
lines from photos. Branch `outfit-kit`. Tools: `Tools/outfit_kit/`.

## Per outfit

`SourceArt/Character/OutfitKit/<Name>/`:

| File | Who | What |
|---|---|---|
| `sketch.blend` | Tema | the drawing, on her default body |
| `brief.md` | Tema | fabrics and colours per layer, what hangs, what to take from each reference |
| `refs/` | Tema | reference photos |
| `sketch.json` | export | every piece as points glued to the skin (vertex ids + barycentric), normals, length, symmetry |
| `sketch.md` | export | the same in words: where each piece starts, ends, its top and bottom, against her landmarks |
| `preview.png` | export | front, her left, back, 3/4, every piece numbered in its layer colour |

## Steps

```
Tools/outfit_kit/kit.sh draw <Name>     # makes the folder the first time, opens Blender in Draw mode
Tools/outfit_kit/kit.sh export <Name>   # after saving: sketch.json, sketch.md, preview.png
```

Drawing: strokes land on her skin (surface placement). Pick the layer first (layer list in
the Draw-mode header, or Object Data properties). Draw her left side (your right in the front
view); the Mirror modifier shows the other half and export mirrors it. Rename a layer to change
its fabric or width: `panel Lace`, `panel Latex bind`, `strap 0.6`, `taut 1.5`, `point buckle`.
Duplicate a layer for a second fabric. Anything that is not geometry goes on `notes`.

| Layer | Draw | Becomes |
|---|---|---|
| `panel <Slot> [bind]` | closed outline | a fabric area; `bind` = bound edges |
| `strap <cm>` | line | a tape lying on the skin |
| `taut <cm>` | line | a strap pulled straight between its ends |
| `hang` | line from its top | a piece hanging under gravity (tie, chain) |
| `seam` | line | a flat seam / boning on a panel |
| `point <part>` | dot | hardware: ring, buckle, slider, clip, bow |
| `notes` | anything | read by agents, ignored by the builder |
| `guides` | (locked) | neck, bust, underbust, waist, hip, crotch, thigh, knee, centre line |

Symmetry: a stroke touching the centre line joins its mirror (half a waistband = the whole
band; half a brief outline = one closed outline). A stroke crossing the centre is kept. A
stroke off the centre becomes a left/right pair. Turn the Mirror modifier off for asymmetry.
Export warns when a stroke snapped onto another body part (a jump between points).

## Next

1. Builder: `blender_outfit.py` reads `sketch.json` (panels → `region` fields, strap/taut/hang
   → `tube`/`taut`/`hanging_tie`, points → hardware), so a new outfit is a sketch + brief, not new code.
2. Check: overlay the drawing on the built garment, report the max deviation in cm.
3. Shop renders: garment-only studio captures in UE for the wardrobe cards.
