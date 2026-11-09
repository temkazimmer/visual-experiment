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

1. `Tools/blender_outfit.py` (Blender, headless, about 15 s for all six) cuts every piece
   from `SourceArt/Character/android_skin.fbx`, so it fits her exactly.
   - A piece is a coverage field in cm; it is cut cleanly through the triangles.
   - It designs on her **default body** (Bust and Glutes at 0.55): the skin's rest shape is
     flat and the bust only exists in the morph. The rest shape is derived at the end.
   - Each vertex copies bone weights and **every shape key** from the nearest skin, so
     animation, the body sliders and the jiggle drive the clothes.
   - UV0 is a garment coordinate in cm, so lace and fishnet wrap round her without a seam.
2. `python3 Tools/ue.py Tools/char_outfit.py` imports the six meshes and builds the
   materials: one base per fabric (`M_OutfitSheer`, `Lace`, `Net`, `Satin`, `Latex`, `Trim`)
   and one instance per outfit and fabric. Colours and weights: `WARDROBE` in that file.
3. She wears an outfit through its row in `FVisexBodyProfile::Outfits()`, picked on the
   DRESS tab or with `UVisexBodyComponent::SetOutfit`. Game worlds only.

**When the skin FBX changes** (new sculpt, new morphs such as thigh gap or abs): re-run
steps 1 and 2. New morphs come across by name.

## Checked

In Play in `Lvl_Dust`: all six standing, front and back. Jog with Bust and Glutes at
maximum for Fishnet, Catsuit and Garter.

## Known limits

- Clothes are skinned, not simulated: nothing flutters in the wind yet.
- Bone sliders scale hardware and piping with the body (a wider hip widens the belt).
- Sheer and lace cast no shadow.
- Where a strap crosses a moving joint (garters at the hip in a run) a sliver of skin can show.
- Fishnet and lace cells stretch with girth: wider on the thigh, tighter at the ankle.
- The old flat-build materials (`Content/Character/Android/M_Fabric*`,
  `Content/Character/Outfits/MI_*`) are no longer used by any outfit.
- Shots: `Tools/char_play_shot.py` (in Play; set `OUTFIT = "Noir"`).
