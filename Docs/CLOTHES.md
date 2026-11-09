# Clothes

Branch `character-clothes` (cut from `character-animation`). The clothes session owns the
outfit tools and assets: `Tools/blender_outfit.py`, `Tools/char_outfit.py`,
`Tools/char_play_shot.py`, `SourceArt/Character/Outfits/`, `Content/Character/Outfits/`,
`Content/Character/Clothes/`. The skin, body sliders and Character page belong to the
character session. The only touch on `VisexBody*` is the "Sheer" row in the wardrobe table
(`VisexBodyProfile.cpp`).

## Direction (Tema, 2026-11-09)

Lingerie and anime-catsuit references: sheer mesh, lace, fishnet, satin and latex, garter
belts and straps, stockings, piped edges. Feedback on the first flat versions: "all outfits
lacking depth". So every outfit is built in layers that sit at different heights off the
skin, with thickness on opaque pieces, piping, raised seams, strap geometry and hardware.

## The wardrobe

| Name | What it is | Fabrics |
|---|---|---|
| Noir | Sheer bodysuit with boning, lace cups, satin band and straps, seamed thigh-highs | Sheer, Lace, Satin, Trim |
| Garter | Lace balconette and briefs, wide lace garter belt, four straps, stockings | Lace, Sheer, Satin, Trim (gold) |
| Cocoa | Brown satin bra and thong, slim belt, front straps to buckled thigh bands, choker | Satin, Trim (gold) |
| Fishnet | Fishnet tights with a satin waistband, satin bra and high thong | Net, Satin, Trim |
| Catsuit | Navy latex suit with waist and chest cutouts, white piping, stripes, collar, cuffs, belt | Latex, Trim (white) |
| Sheer | High-neck sheer bodysuit, latex yoke, bands and garter belt, six straps, opera gloves | Sheer, Latex, Trim |

## How it is made

1. `Tools/blender_outfit.py` (Blender, headless, about 20 s for all six) cuts every piece
   from `SourceArt/Character/android_skin.fbx`, so it fits her exactly.
   - A piece is a coverage field in cm; it is cut cleanly through the triangles.
   - It designs on her **default body** (Bust 0.55, Glutes 0.55, Fat 0.20, Thigh gap 0.35):
     the skin's rest shape is flat and the bust only exists in the morph. The rest shape is
     derived at the end. If those slider defaults change, change `DESIGN` to match.
   - Fabric vertices take the weights and **every shape key** of the exact skin vertices
     they were cut from, so cloth cannot sink into the body at a joint. Cords, tapes,
     straps and hardware ride the single skin vertex beneath them.
   - UV0 is a garment coordinate in cm, so lace and fishnet wrap round her without a seam.
2. `Tools/blender_outfit_textures.py` writes three tileable 2K detail textures: a satin
   weave normal, a latex ripple normal and a mask for roughness variation.
3. `python3 Tools/ue.py Tools/char_outfit.py` imports textures and meshes and builds the
   materials: one base per fabric (`M_OutfitSheer`, `Lace`, `Net`, `Satin`, `Latex`, `Trim`)
   and one instance per outfit and fabric. Colours and weights: `WARDROBE` in that file.
4. She wears an outfit through its row in `FVisexBodyProfile::Outfits()`, picked on the
   DRESS tab or with `UVisexBodyComponent::SetOutfit`. Game worlds only. Her default
   outfit is "Nothing": a fresh Play shows her bare until one is picked.

**When the skin FBX changes** (new sculpt or morphs): re-run steps 1 and 3. New morphs come
across by name.

## Render quality (2560 x 1440)

- **Textures:** 2048 px, linear, normal or masks compression, never streamed, LOD bias 0.
  Lace, fishnet and the tulle weave of sheer cloth are computed in the material from the
  garment UV, so they stay sharp at any distance.
- **Latex:** clear coat with a near-mirror coat (roughness 0.04 to 0.06), soft ripples in
  the base normal, smudges that dull the coat a little in places.
- **Satin and trim:** weave normal, roughness variation; each outfit sets its own amounts.
- **Sheer and lace:** translucent, lit per pixel, closing up at grazing angles; up close
  the tint resolves into a tulle mesh. They write motion vectors, so they do not smear.
- **Fishnet:** masked cords with a rounded normal.
- **Mesh:** 31k to 183k vertices per outfit, one LOD, smooth normals.
- **Seams are flat tapes**, not raised cords: a raised cord in this low sun puts a row of
  dark dashes on the skin beside it. A piped edge must not run under a band for the same
  kind of reason: it flickers through.
- Proof frames: `Docs/clothes-2k/` (HighResShot from Play, standing and walking).

## Checked

In Play in `Lvl_Dust` at 2560 x 1440, after Start: all six standing, full body and close.
Walking with the game's own walk (not a looped clip): Noir at hips and knees, Catsuit at
hips. No skin showed through in those frames.

## Known limits

- Clothes are skinned, not simulated: nothing flutters in the wind yet.
- Bone sliders scale hardware and piping with the body (a wider hip widens the belt).
- Sheer and lace cast no shadow.
- Garter straps are skinned ribbons: in a deep hip bend they can cut into the thigh.
- Fishnet and lace cells stretch with girth: wider on the thigh, tighter at the ankle.
- The old flat-build materials (`Content/Character/Android/M_Fabric*`,
  `Content/Character/Outfits/MI_*`) are no longer used by any outfit.
- The outfit component is created by the body component and casts no shadow at all.
- Shots: `Tools/char_play_shot.py` (in Play: `START`, `OUTFIT`, `WALK`, `SHOT`).
