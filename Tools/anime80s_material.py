# Builds /Game/VisEx/UI/M_Anime80s: the "80s anime" look for Settings > Visuals.
# A post-process after tonemapping: brightness quantised into a few soft cel bands (hue kept),
# ink outlines where depth or the surface normal breaks, fading out with distance so the far
# dunes and sky stay clean, and a saturation lift.
#   python3 Tools/ue.py Tools/anime80s_material.py      (with Play stopped)
import unreal

PATH, NAME = "/Game/VisEx/UI", "M_Anime80s"

BANDS = 4.0          # brightness steps
SATURATION = 1.35
THICKNESS = 1.5      # outline width, px at 1080p (scaled with the buffer)
DEPTH_T = 0.025      # relative depth jump that counts as an edge
NORMAL_T = 0.4       # 1 - cos(angle) that counts as a crease
FAR_FADE = 6000.0    # cm: outlines gone by here
INK = (0.09, 0.04, 0.10)

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
full = f"{PATH}/{NAME}"
# load first: create_asset on an existing name opens a modal Overwrite dialog and blocks the editor
m = unreal.load_asset(full) or tools.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mel.delete_all_material_expressions(m)
m.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)


def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


# scene-texture nodes, so the lookups in the code below are bound (fed in as unused inputs)
ids = [("Color", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),
       ("Depth", unreal.SceneTextureId.PPI_SCENE_DEPTH),
       ("Normal", unreal.SceneTextureId.PPI_WORLD_NORMAL),
       ("Stencil", unreal.SceneTextureId.PPI_CUSTOM_STENCIL)]
custom = node(unreal.MaterialExpressionCustom, -200, 0)
inputs = []
for i, (name, tex_id) in enumerate(ids):
    t = node(unreal.MaterialExpressionSceneTexture, -600, i * 200)
    t.set_editor_property("scene_texture_id", tex_id)
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", name)
    inputs.append((ci, t))
custom.set_editor_property("inputs", [ci for ci, _ in inputs])
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property("code", f"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 px = View.BufferSizeAndInvSize.zw * {THICKNESS} * max(1.0, View.BufferSizeAndInvSize.y / 1080.0);
float3 c = SceneTextureLookup(uv, 14, false).rgb;
// stencil 1: the menu panels' text (Tools/menu_panel_material.py) stays as it is
if (Stencil.r > 0.5) return c;
float d0 = SceneTextureLookup(uv, 1, false).r;
float3 n0 = SceneTextureLookup(uv, 8, false).rgb;
float edge = 0;
float2 offs[4] = {{ float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1) }};
for (int i = 0; i < 4; i++)
{{
    float2 o = uv + offs[i] * px;
    float d = SceneTextureLookup(o, 1, false).r;
    float3 n = SceneTextureLookup(o, 8, false).rgb;
    float near = min(d, d0);
    edge = max(edge, saturate((abs(d - d0) / max(near, 1.0) - {DEPTH_T}) * 25.0));
    edge = max(edge, saturate(((1.0 - dot(n, n0)) - {NORMAL_T}) * 4.0) * step(near, 1e6));
}}
edge *= saturate(1.0 - d0 / {FAR_FADE});

float L = dot(c, float3(0.299, 0.587, 0.114));
// the band is picked from a softened brightness, so fog gradients and noise don't break it into specks
float Ls = L * 0.2;
float2 soft[8] = {{ float2(3, 0), float2(-3, 0), float2(0, 3), float2(0, -3), float2(2, 2), float2(-2, 2), float2(2, -2), float2(-2, -2) }};
for (int j = 0; j < 8; j++)
{{
    Ls += dot(SceneTextureLookup(uv + soft[j] * px, 14, false).rgb, float3(0.299, 0.587, 0.114)) * 0.1;
}}
float q = Ls * {BANDS};
float B = max((floor(q) + smoothstep(0.42, 0.58, frac(q))) / {BANDS}, 0.35 / {BANDS});
float3 cel = c * (B / max(L, 1e-4));
float L2 = dot(cel, float3(0.299, 0.587, 0.114));
cel = max(lerp(L2.xxx, cel, {SATURATION}), 0);
return lerp(cel, float3{INK}, edge) + (Color.r + Depth.r + Normal.r + Stencil.r) * 0;
""")
for ci, t in inputs:
    mel.connect_material_expressions(t, "Color", custom, ci.get_editor_property("input_name"))
mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)
unreal.EditorAssetLibrary.save_asset(full)
print("built", full)
