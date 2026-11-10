# Look research: realistic desert dust-fog (2026-11-10)

This is web research on how desert, dust and fog scenes like Blade Runner 2049's Las Vegas, Dune and UE5 environment breakdowns get a realistic picture. Values marked [EK] come from engine knowledge; no source opened during the research confirms them.

## Blending into endless sand
- **Fog colour from the sky.** Turn on the project setting *Support Sky Atmosphere Affecting Height Fog*. Below the horizon, Sky Atmosphere renders black, so fog or a far mesh must fill it. ([WoLD desert breakdown](https://www.worldofleveldesign.com/categories/ue4/breakdown-desert-dunes-pyramids.php))
- **Volumetric fog replaces the height-fog inscattering colour within its View Distance.** Past that distance the height-fog colour takes over. Volumetric fog ignores Start Distance, Max Opacity and Cutoff. Match the two colours, or a band shows at the View Distance. ([Epic: volumetric fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine))
- **Height-fog tools:** Second Fog Data, Directional Inscattering toward the sun, and an Inscattering Colour Cubemap so distant fog matches the sky exactly. ([Epic: height fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/exponential-height-fog-in-unreal-engine))
- **Far dune skirt.** Place it far enough out that it never pops, swallowed by fog. Tiling stops showing under fog. Fade the sand's fine detail with distance. ([polycount](https://polycount.com/discussion/comment/2566653))

## Atmosphere
- **BR2049 got the orange in camera.** Deakins used a custom Tiffen filter pack built from Lee gels 790 Moroccan Pink, 105 Orange, 134 Golden Amber and 020 Medium Amber. The UE equivalent is a warm amber/pink grade on a near-neutral render, not just saturated fog. ([ECG](https://www.ecgprod.com/lighting-design-blade-runner-2049/))
- **Grade the haze by distance.** Uniform orange haze over the distance "looks like an effect". Keep it less orange at range and dense orange in the mid ground. ([vfxblog](https://vfxblog.com/2017/10/25/what-happens-in-vegas))
- **Framestore matched the city in neutral white light first,** then added orange and sand. ([vfxblog](https://vfxblog.com/2018/01/31/blade-runner-2049-las-vegas-framestore))
- **Haze from the sky:** raise Mie Scattering Scale. The WoLD breakdown used a 5000 K sun at about 7 lux, SkyLight intensity 2, a fixed exposure, and light vignette and chromatic aberration.
- **God rays:** volumetric fog plus the directional light's Volumetric Scattering Intensity. A Scattering Distribution near 1 pushes the scattering toward the sun.

## Sand and particles
- Macro variation against tiling ([versluis](https://versluis.com/2023/05/how-to-use-macro-variations-for-unreal-engine-landscape-materials)).
- Ripple and grain normals combined with BlendAngleCorrectedNormals.
- Multi-scale glints ([forum](https://forums.unrealengine.com/t/sparkling-material/52436?page=2)).
- **Wind-blown sand:** a ground-hugging Niagara sand sheet plus streamers off dune crests, driven by a wind parameter ([80.lv wind](https://80.lv/articles/a-new-tutorial-explains-how-to-make-realistic-wind-in-unreal-engine-5)).
- **Fog cards:** two panned noises, Depth Fade against geometry, Distance Blend as the camera nears ([Rookies](https://www.therookies.co/blog/creating-open-world-environments-in-unreal-engine-5-part-3/)).
- **Dust lighting:** lit translucency so dust takes the sun and the fog colour; unlit sprites look pasted on [EK].
- **Local fog without Local Fog Volumes:** Volume-domain particle materials write density into volumetric fog.

## Scale cues
- **Human-scale props:** Framestore added tables, chairs and street furniture because detail vanishes in the haze ([Foundry](https://www.foundry.com/insights/film-tv/iconic-look-blade-runner-2049)).
- **Layers:** foreground, mid ground and background, each lighter and lower in contrast than the last.
- **Surface detail rhythm:** plain geometric forms read as fake.

## Top 8 for Lvl_Dust
1. Match the volumetric-fog, height-fog and stone-haze colours, and keep Sky Atmosphere feeding the fog.
2. Grade by distance: a neutral, contrasty foreground; orange in the mid ground; a lighter, desaturated distance.
3. Make the orange a warm amber/pink grade on a near-neutral render (Deakins), not only saturated fog and sun.
4. Add a ground-hugging Niagara sand sheet plus crest streamers, with lit translucency, depth fade and wind.
5. Far dune skirt swallowed by fog. *Done 2026-11-10: `add_dunes.py` endless field.*
6. Sand material: macro variation, angle-corrected ripples, distance-faded micro detail, multi-scale glints.
7. Volume-domain Niagara fog puffs between the columns.
8. Human-scale props at the column bases; fixed exposure, light grain and chromatic aberration.
