# Clothes

Branch `character-clothes` (cut from `character-animation`). The clothes session owns the
outfit tools and assets: `Tools/blender_outfit.py`, `Tools/char_outfit.py`,
`Tools/char_play_shot.py`, `SourceArt/Character/Outfits/`, `Content/Character/Outfits/`,
`Content/Character/Clothes/`. The skin, body sliders and Character page belong to the
character session. The only touch on `VisexBody*` is the wardrobe table and the default outfit
(`VisexBodyProfile.cpp`).

## Direction (Tema, 2026-11-09)

Lingerie and anime-catsuit references: sheer mesh, lace, fishnet, satin and latex, garter
belts and straps, stockings, piped edges. Feedback on the first flat versions: "all outfits
lacking depth". So every outfit is built in layers that sit at different heights off the
skin, with thickness on opaque pieces, piping, raised seams, strap geometry and hardware.

## The wardrobe

**One outfit for now** (Tema, 2026-11-09): the DRESS tab lists only "Nothing" and "Sheer
bodystocking", and the bodystocking is her default. The other six below are still built,
imported and on disk, but unlisted; add a row in `FVisexBodyProfile::Outfits()` to bring
one back.

| Name | What it is | Fabrics |
|---|---|---|
| Noir | Sheer bodysuit with boning, lace cups, satin band and straps, seamed thigh-highs | Sheer, Lace, Satin, Trim |
| Garter | Lace balconette and briefs, wide lace garter belt, four straps, stockings | Lace, Sheer, Satin, Trim (gold) |
| Cocoa | Brown satin bra and thong, slim belt, front straps to buckled thigh bands, choker | Satin, Trim (gold) |
| Fishnet | Fishnet tights with a satin waistband, satin bra and high thong | Net, Satin, Trim |
| Catsuit | Navy latex suit with waist and chest cutouts, white piping, stripes, collar, cuffs, belt | Latex, Trim (white) |
| Sheer | High-neck sheer bodysuit, latex yoke, bands and garter belt, six straps, opera gloves | Sheer, Latex, Trim |
| **Bodystocking** | The hero outfit: one piece of sheer black nylon. Scoop tank, deep V back, open hips, open gusset, strap-held stocking legs, gold chain and bangles | Nylon, Binding, Trim (gold) |

## The hero outfit: Bodystocking (Tema, 2026-11-09)

"Let's make one outfit but with best quality", from three reference photos of a sheer
black open-hip bodystocking. What it has that the others do not:

- **Nylon material** (`M_OutfitNylon`): coverage is computed the way a knit behaves. Head-on
  you see through the loops; along the surface you look through more yarn, so it goes
  black toward every silhouette. Where the knit is pulled open (bust, glutes, thighs) it
  thins; ankles are denser. Legs are a heavier denier than the top. The mesh carries this
  as UV1 (stretch, leg-ness).
- **Tight fit:** the cloth spans small hollows (navel, spine, creases) instead of sinking
  into them (`BRIDGE`).
- **Double-knit thigh tops**, a second layer from the hip crease down.
- **Bound edges** (a 3 mm flat tape on every opening) and centre seams front and back.
- **Jewellery:** a fine gold chain on the collarbones, bangles loose on the wrists.
- Pattern values are in `bodystocking()` in `Tools/blender_outfit.py`; cover values in
  `WARDROBE["Bodystocking"]` in `Tools/char_outfit.py`. Frames: `Docs/clothes-2k/bodystocking/`.
- Not done: the references' heels (needs a foot pose, which is the animation lane), and
  hair or a face to hang earrings on.

## How it is made

1. `Tools/blender_outfit.py` (Blender, headless, about 20 s for all six) cuts every piece
   from `SourceArt/Character/android_skin.fbx`, so it fits her exactly.
   - A piece is a coverage field in cm; it is cut cleanly through the triangles.
   - It designs on her **default body** (Bust 0.55, Glutes 0.55, Fat 0.20, Thigh gap 0.35):
     the skin's rest shape is flat and the bust only exists in the morph. The rest shape is
     derived at the end. If those slider defaults change, change `DESIGN` to match.
   - Fabric vertices take **all** the weights (up to 8 per vertex: the hips carry 7; cutting
     to 4 let skin show through in slivers) and **every shape key** of the exact skin
     vertices they were cut from, so cloth cannot sink into the body at a joint. Cords, tapes,
     straps and hardware ride the single skin vertex beneath them.
   - UV0 is a garment coordinate in cm, so lace and fishnet wrap round her without a seam.
2. `Tools/blender_outfit_textures.py` writes three tileable 2K detail textures: a satin
   weave normal, a latex ripple normal and a mask for roughness variation.
3. `python3 Tools/ue.py Tools/char_outfit.py` imports textures and meshes and builds the
   materials: one base per fabric (`M_OutfitSheer`, `Lace`, `Net`, `Satin`, `Latex`, `Trim`)
   and one instance per outfit and fabric. Colours and weights: `WARDROBE` in that file.
4. She wears an outfit through its row in `FVisexBodyProfile::Outfits()`, picked on the
   DRESS tab or with `UVisexBodyComponent::SetOutfit`. Game worlds only. Her default
   outfit is "Bodystocking" (`FVisexBodyProfile::Outfit`). Saves store the outfit only when
   it differs from the default: a save from before the default loads as the bodystocking, a
   saved "Nothing" stays nothing, and a save naming an unlisted outfit shows nothing.

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
- The FBX also carries her hair; the builder drops the "Hair" material before fitting.
- Shots: `Tools/char_play_shot.py` (in Play: `START`, `OUTFIT`, `WALK`, `SHOT`).
