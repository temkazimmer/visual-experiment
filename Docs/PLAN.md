# vis-ex (visual-experiment) — first prototype plan

A UE5 game about posing 3D models in different outfits and surroundings, then
taking photos through filters (CRT monitor, 80s anime, low poly) that can each be
turned on or off and stacked.

**Engine:** Unreal Engine 5.7.4, Blueprints only — no C++ until something needs it.
**Machine:** M5 Max / 128 GB, so run everything on the highest quality settings.

**The prototype:** one model, one pose, one scene, one filter you can toggle, and a
way to save the shot. Every phase ends with a picture you can keep.

> UE menu names shift a little between versions. If a label below doesn't match
> what 5.7 shows, look for the nearest equivalent, or send a screenshot.

---

## Phase 0 — Project (≈30 min)

1. **New Project → Games → Third Person**, Blueprint, Starter Content **on**,
   Quality **Maximum**. Named `visex`.
2. Press **Play**, run around, then press **Esc**.
3. Open the console (the **`** key) and type `HighResShot 2`.

**You'll have:** your first screenshot in `visex/Saved/Screenshots/`.

## Phase 1 — One posed model, one shot (an evening)

1. In the Content Browser, find the mannequin `SKM_Manny` (or `SKM_Quinn`) and drag
   it into the level. This creates a **Skeletal Mesh Actor**.
2. In Details → **Animation**: set Animation Mode to **Use Animation Asset**, pick
   any animation from the template (idle, jump…), and untick **Looping** and
   **Playing**. Change **Initial Position** to freeze a different frame. That
   frozen frame is your pose.
3. Add a **Cine Camera Actor**. Right-click it and choose **Pilot**, then frame the
   shot. Try focal length 50–85 mm and enable **Focus Settings → Tracking** on the
   model for a shallow depth of field.
4. Light it: move the Directional Light for a low sun, and add a Rect Light as a
   key light.
5. Take the shot with `HighResShot 2` while piloting the camera.

**You'll have:** a posed, lit, framed photo — the core loop, done by hand.

## Phase 2 — First filter: CRT (a weekend)

Filters are **post-process materials**: a material that redraws the whole frame.

1. Create a material `M_Filter_CRT`, and set **Material Domain → Post Process**.
2. Build the graph from these four pieces, each one a few nodes:
   - **Base image:** `SceneTexture: PostProcessInput0`.
   - **Scanlines:** `ScreenPosition`, take the V component, multiply by ~2000,
     then `Sine`, remap to 0–1, then `Lerp(1, it, 0.25)`, multiplied into the color.
   - **RGB split:** sample the scene texture again with the UV offset ±0.001 on X,
     and take R from one sample and B from the other.
   - **Vignette:** darken by distance from the screen centre (UV − 0.5).
   - Send the result to **Emissive Color**.
3. Add a **Post Process Volume** and tick **Infinite Extent (Unbound)**. Under
   **Rendering Features → Post Process Materials**, add `M_Filter_CRT`.
4. **Toggle:** open the Level Blueprint, add a `1` key event that flips a boolean,
   then call **Add or Update Blendable** on the volume with Weight = 1 or 0.

**You'll have:** the same photo with the CRT look on and off, side by side.

*This is the end of the prototype: one model, one pose, one scene, one toggle.*

---

## After the prototype — sketched, not specified

Each of these is about a weekend, and each one is useful on its own. The order is
a suggestion.

- **Photo mode in-game.** A `P` key freezes the character (Custom Time Dilation
  0), possesses a free-flying camera, hides the HUD, and binds a key to
  `HighResShot`. Later: sliders for focal length, aperture and exposure (UMG).
- **More filters, stackable.** Make each filter a separate post-process material
  with its own weight, on keys `1` `2` `3`.
  - **80s anime:** cel-shade by quantising brightness into 3–4 bands, add ink
    outlines with edge detection on SceneDepth and WorldNormal, and boost
    saturation. This is the hardest of the three.
  - **Low poly:** true low poly means different geometry. A screen filter can only
    fake it, with pixelation, colour posterising and a slight vertex wobble
    (the PS1 look). Decide which version you want when you get here.
  - Stacking order matters: anime → low poly → CRT reads best, because the CRT
    should sit "on the glass".
- **Pose library.** Use free Mixamo animations, retarget them onto Manny
  (right-click → Retarget Animations), and cycle poses with a key. Later, author
  your own poses with Control Rig and save them as Pose Assets.
- **Outfits.** Modular clothing meshes that follow the body's animation (Leader
  Pose Component), swapped with a key. Fab has free outfits; MetaHuman is the
  higher-quality route.
- **Surroundings.** Make 2–3 small levels (studio cyclorama, street at night,
  nature) and switch with **Open Level**. Free environment packs on Fab make this
  fast.
- **Later, if it's still fun:** a menu (UMG), an in-game gallery of saved shots,
  and saved presets of pose + outfit + scene + filters.
