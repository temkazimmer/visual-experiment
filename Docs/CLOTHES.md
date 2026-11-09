# Clothes

Branch `character-clothes` (cut from `character-animation`). Owns `Content/Character/Clothes/`,
`SourceArt/Character/Clothes/`, `Tools/blender_outfit.py`, `Tools/char_outfit*.py`,
`Tools/char_play_shot.py`, and since the character session paused fabric work also
`Tools/blender_outfits.py`, `Tools/char_outfits.py`, `Content/Character/Outfits/`. The only
touch on `VisexBody*` is one wardrobe row in `VisexBodyProfile.cpp`.

## Direction (Tema, 2026-11-09)

Lingerie and anime-catsuit references: sheer mesh bodysuit, opaque latex yoke and bands,
garter belt and straps, stockings, piped edges. First feedback on flat proxies: "all
outfits lacking depth". So every outfit is built in layers that sit at different heights
off the skin, with raised seams, piping, straps and hardware.

## Outfit 1: `SKM_Outfit_Sheer`

| Layer | Offset | Slot |
|---|---|---|
| Sheer bodysuit (high neck, high-cut leg), stockings | 0.2–0.25 cm | Sheer |
| Gloves, yoke, underbust band, garter belt, welts | 0.3–0.75 cm | Latex |
| Boning seams, stocking back seams, garter straps | ~0.5–0.9 cm | Latex |
| Piping on every edge, garter clips | on the edges | Trim |

## How it is made

1. `Tools/blender_outfit.py` (Blender, headless) cuts every piece from
   `SourceArt/Character/android_skin.fbx`, so it fits her exactly. Each vertex copies skin
   weights and **every shape key** from the nearest skin, so animation and the body-slider
   morphs drive the clothes. Look values: `FIT` at the top.
2. `python3 Tools/ue.py Tools/char_outfit.py` imports it and builds `M_OutfitSheer`
   (translucent, closes up at grazing angles), `M_OutfitLatex` (clear coat),
   `M_OutfitTrim` (metal). Look values: `LOOK`.
3. She wears it through the character branch's wardrobe: one row ("Sheer") in
   `FVisexBodyProfile::Outfits()`, picked on the DRESS tab or with
   `UVisexBodyComponent::SetOutfit`. The body component owns the outfit component, which
   follows the body by Leader Pose. Game worlds only: she is not dressed in the editor world.

**When the skin FBX changes** (new sculpt, new morphs such as abs): re-run steps 1 and 2.

## Checked

In Play in `Lvl_Dust`: idle, jog, and Bust/Glutes/Hips/Thighs at maximum with Waist at
minimum. The outfit follows with no skin poking through at those values.

## Known limits

- Clothes are skinned, not simulated: nothing flutters in the wind yet.
- Bone sliders scale the clips and piping with the body (a wider hip widens the belt).
- The sheer layer casts no shadow. Translucency sorts per component, which is fine while
  skin always sits between two sheer surfaces.
- Shots: `Tools/char_play_shot.py` (in Play; set `OUTFIT = "Sheer"`).
