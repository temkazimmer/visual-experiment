# visual-experiment

UE5 test visualisation: a posing / photo-mode playground. The first scene, `Lvl_Dust`, is an
orange dust-fog plaza: giant ruined colonnade, stepped plaza, wind-shaped dunes.

Unreal Engine 5.7 on macOS (Apple silicon). Project name: `visex`.

## What's here

| Path | |
|---|---|
| `Content/VisEx/` | Levels (`Lvl_Dust`, `Lvl_Studio`), materials, dunes, baked stone maps, weathered stone meshes |
| `Content/Python/init_unreal.py` | Folder bridge: runs `.py` files dropped into `Tools/.bridge/in/` inside the open editor |
| `Tools/` | Everything is built by script — see below |
| `SourceArt/VisEx/Stone/` | Our own baked stone textures (PNG sources) |

Binary assets (`.uasset`, `.umap`, images) are in Git LFS: `git lfs install` before cloning.

## Fab / Megascans assets (not included)

These are licensed through Fab and can't be redistributed, so they're gitignored. Download them
(free) and put them where the scripts expect them:

| Fab asset | Version | Put files in |
|---|---|---|
| Rippled Sand (`vd3lecfs`) | 8K UE (raw) — `Textures/` folder | `SourceArt/Megascans/RippledSand_vd3lecfs/` |
| Rippled Sand (`vd3lecfs`) | 2K — BaseColor, Normal, Roughness jpgs | `SourceArt/Megascans/RippledSand_vd3lecfs_2k/` |
| Desert Sand (`wkjgche`) | 1K — BaseColor, Normal, Roughness jpgs | `SourceArt/Megascans/DesertSand_wkjgche_1k/` |

Then, with the editor open: `python3 Tools/ue.py Tools/import_sand_tex.py` and
`python3 Tools/ue.py Tools/add_sand.py`. Without them the sand material won't compile.

## Pipeline

All live in the open editor via the bridge (`python3 Tools/ue.py <script>`), unless noted.

1. `build_dust.py` — the level from scratch (commandlet, editor closed; see its docstring)
2. `import_sand_tex.py` — Fab sand textures
3. `bake_stone.py` — bake our tileable wall/slab texture sets (run twice: the first run compiles the generator)
4. `add_sand.py` — `M_SandCover`: sand settling on stone, ripples, glints, baked stone maps
5. `add_dunes.py` — heightfield dune field, walkable
6. `add_ruins.py` — snapped and toppled columns, rubble
7. dump actors to `Tools/.bridge/actors.json`, then `python3 Tools/stone_geo.py` (outside the
   editor) and `apply_geo.py` — weathered Nanite geometry: chipped edges, grooved joints, drum
   columns, separate floor slabs
8. `capture.py` — render a shot to `Tools/.bridge/capture.png`

The mannequin (`Content/Characters`) comes from Epic's Third Person template and is under the
Unreal Engine EULA, not the Unlicense.
