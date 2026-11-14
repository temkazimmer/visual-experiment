"""Paint the MetaHuman's baked underwear out of its body textures (skin under outfits with openings).

The assembly bakes the grey top and briefs into the body's base colour, normal and SRMF maps and
leaves no switch. The plugin ships the underwear's mask in the same UV layout
(/MetaHumanCharacter/Textures/Shared/1K/T_Underwear_M), so the covered skin is filled in from the
skin around it: smooth, with no anatomy of its own (the material's micro detail still runs over it).

  1. in the editor:  python3 Tools/ue.py -c "STEP='export'; exec(open('<project>/Tools/mh_bare_skin.py').read())"
       -> Saved/MH/tex/*.png (the three baked maps and the mask)
  2. outside:        Blender -b --factory-startup --python Tools/mh_bare_skin.py -- Saved/MH/tex SourceArt/Character/MH/Bare
       -> bare_bc.png, bare_n.png, bare_srmf.png (4096; Blender is only the numpy host, ffmpeg does the files)
  3. in the editor:  STEP='import'  -> /Game/Character/MH/Outfits/T_Body_{BC,N,SRMF}_Bare, T_Head_{BC,N,SRMF}_Bare, MI_Head_Bare
Tools/mh_outfit.py then gives every outfit's body material the body maps instead of the baked ones, and
the game puts MI_Head_Bare on the face mesh while an outfit is worn (the head mesh carries the top's
shoulder straps on its chest part).
Run again after a re-assembly (new skin tone, new bake).
"""
import os
import sys

MAPS = {"bc": "T_Body_BC_VT", "n": "T_Body_N_VT", "srmf": "T_Body_SRMF_VT"}
HEAD = {"bc": "T_Head_BC_VT", "n": "T_Head_N_VT", "srmf": "T_Head_SRMF_VT"}      # the face mesh's skin, LOD 0-1
MASK = "T_Underwear_M"
SIZE, LOW = 4096, 1024      # output size; the fill is solved at the mask's own size
GROW = 4                    # mask px (about 0.8 cm): the baked hems shade the skin a little past the mask

try:
    import unreal
except ImportError:
    unreal = None

if unreal:
    _root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    _tex = os.path.join(_root, "Saved", "MH", "tex")
    _baked = "/Game/Character/MH/Built/MHC_Her/Body/Baked/"
    _dir = "/Game/Character/MH/Outfits"
    STEP = globals().get("STEP", "export")
    if STEP == "export":
        os.makedirs(_tex, exist_ok=True)
        for path in [_baked + n for n in MAPS.values()] + ["/Game/Character/MH/Built/MHC_Her/Face/Baked/" + n for n in HEAD.values()] + \
                ["/MetaHumanCharacter/Textures/Shared/1K/" + MASK]:
            t = unreal.load_asset(path)
            assert t, "no texture at " + path
            task = unreal.AssetExportTask()
            task.set_editor_property("object", t)
            task.set_editor_property("filename", os.path.join(_tex, t.get_name() + ".png"))
            task.set_editor_property("automated", True)
            task.set_editor_property("prompt", False)
            task.set_editor_property("replace_identical", True)
            assert unreal.Exporter.run_asset_export_task(task), "export failed: " + path
        print("exported to", _tex)
    elif STEP == "import":
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mel, eal = unreal.MaterialEditingLibrary, unreal.EditorAssetLibrary
        _face = "/Game/Character/MH/Built/MHC_Her/Face/"
        made = {}
        for prefix, maps, folder in (("bare", MAPS, _baked), ("bare_head", HEAD, _face + "Baked/")):
            for key, src_name in maps.items():
                src = unreal.load_asset(folder + src_name)
                name = src_name.replace("_VT", "_Bare")
                task = unreal.AssetImportTask()
                task.set_editor_property("filename", os.path.join(_root, "SourceArt", "Character", "MH", "Bare", "%s_%s.png" % (prefix, key)))
                task.set_editor_property("destination_path", _dir)
                task.set_editor_property("destination_name", name)
                task.set_editor_property("automated", True)
                task.set_editor_property("replace_existing", True)
                task.set_editor_property("save", False)
                tools.import_asset_tasks([task])
                tex = unreal.load_asset(_dir + "/" + name)
                assert tex, "import failed: " + name
                for prop in ("srgb", "compression_settings", "lod_group", "virtual_texture_streaming"):   # as the baked map it replaces
                    tex.set_editor_property(prop, src.get_editor_property(prop))
                eal.save_loaded_asset(tex)
                made[src_name] = tex
                print(name, tex.get_editor_property("compression_settings"), "vt", tex.get_editor_property("virtual_texture_streaming"))
        # the face mesh's LOD 0 skin with the straps gone: UVisexMHBodyComponent puts it on while an outfit is worn
        head = unreal.load_asset(_dir + "/MI_Head_Bare") if eal.does_asset_exist(_dir + "/MI_Head_Bare") else \
            eal.duplicate_asset(_face + "Materials/MI_Face_Skin_Baked_LOD0_VT", _dir + "/MI_Head_Bare")
        for param, src_name in (("Basecolor Baked VT", HEAD["bc"]), ("Normal Baked VT", HEAD["n"]), ("SRMF Baked VT", HEAD["srmf"])):
            mel.set_material_instance_texture_parameter_value(head, param, made[src_name])
        mel.update_material_instance(head)
        eal.save_loaded_asset(head)
        print("head material", head.get_path_name())
else:
    import subprocess
    import numpy as np

    args = sys.argv[sys.argv.index("--") + 1:]
    SRC, OUT = args[0], args[1]
    os.makedirs(OUT, exist_ok=True)

    def read(path, size):
        raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", path, "-vf", "scale=%d:%d:flags=area" % (size, size),
                              "-f", "rawvideo", "-pix_fmt", "rgba", "-"], capture_output=True, check=True).stdout
        return np.frombuffer(raw, np.uint8).reshape(size, size, 4).astype(np.float32) / 255.0

    def resize(img, size):
        n = img.shape[0]
        raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", "%dx%d" % (n, n), "-i", "-",
                              "-vf", "scale=%d:%d:flags=bicubic" % (size, size), "-f", "rawvideo", "-pix_fmt", "rgba", "-"],
                             input=(np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8).tobytes(), capture_output=True, check=True).stdout
        return np.frombuffer(raw, np.uint8).reshape(size, size, 4).astype(np.float32) / 255.0

    def write(img, path):
        n = img.shape[0]
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", "%dx%d" % (n, n), "-i", "-", path],
                       input=(np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8).tobytes(), check=True)

    def grow(mask, px):
        for _ in range(px):
            m = mask.copy()
            m[1:] |= mask[:-1]; m[:-1] |= mask[1:]; m[:, 1:] |= mask[:, :-1]; m[:, :-1] |= mask[:, 1:]
            mask = m
        return mask

    def fill(img, hole, pad, relax=300):
        """Skin under the hole from the skin round it: a pull-push pyramid for the far field, then a
        Laplace relaxation so the fill has no blocks and meets the edge smoothly. pad: texels outside
        the UV islands (a flat brown), which must not bleed in."""
        pad = pad.copy()
        pad[:3] = pad[-3:] = pad[:, :3] = pad[:, -3:] = True     # nor the map's own edge rows (bleed colours, and np.roll wraps)
        w = (~hole & ~pad).astype(np.float32)
        pyr = [(img * w[..., None], w)]
        while pyr[-1][1].shape[0] > 1:
            a, b = pyr[-1]
            n = a.shape[0] // 2
            pyr.append((a.reshape(n, 2, n, 2, 4).sum((1, 3)), b.reshape(n, 2, n, 2).sum((1, 3))))
        cur = pyr[-1][0] / max(float(pyr[-1][1][0, 0]), 1e-6)
        for a, b in reversed(pyr[:-1]):
            up = np.repeat(np.repeat(cur, 2, 0), 2, 1)
            cur = np.where(b[..., None] > 0, a / np.maximum(b, 1e-6)[..., None], up)
        out = np.where(hole[..., None], cur, img)
        ok = (~pad | hole).astype(np.float32)[..., None]
        near = lambda a: np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)
        cnt = np.maximum(near(ok), 1e-6)
        for _ in range(relax):
            out = np.where(hole[..., None], near(out * ok) / cnt, img)
        return out

    def paint(maps, hole, prefix):
        soft = resize(np.repeat(hole[..., None].astype(np.float32), 4, 2), SIZE)[..., :1]
        soft = np.clip((soft - 0.3) / 0.4, 0, 1)
        soft = soft * soft * (3 - 2 * soft)
        # the padding outside the UV islands is one flat colour in the base colour map: its most common value
        bc8 = (read(os.path.join(SRC, maps["bc"] + ".png"), LOW)[..., :3] * 255 + 0.5).astype(np.int32)
        code = bc8[..., 0] * 65536 + bc8[..., 1] * 256 + bc8[..., 2]
        vals, counts = np.unique(code[~hole], return_counts=True)
        top = int(vals[np.argmax(counts)])
        flat = np.array([top // 65536, (top // 256) % 256, top % 256])
        pad = grow((np.abs(bc8 - flat).max(-1) <= 2) & ~hole, 2) & ~hole
        if pad.mean() > 0.3:          # no padding in this map (the head's fills its square): that was skin
            pad[:] = False
        print(prefix, "underwear covers %.1f%% of the map, padding %.1f%%" % (100.0 * hole.mean(), 100.0 * pad.mean()))
        for key, name in maps.items():
            low = fill(read(os.path.join(SRC, name + ".png"), LOW), hole, pad)
            full = read(os.path.join(SRC, name + ".png"), SIZE)
            write(full * (1 - soft) + resize(low, SIZE) * soft, os.path.join(OUT, "%s_%s.png" % (prefix, key)))
            print("wrote", os.path.join(OUT, "%s_%s.png" % (prefix, key)))

    # body: the plugin's own mask (red and green: the top, the briefs' front, sides and back)
    m = read(os.path.join(SRC, MASK + ".png"), LOW)
    paint(MAPS, grow((m[..., 0] > 0.5) | (m[..., 1] > 0.5), GROW), "bare")
    # head: its chest part carries the top's shoulder straps, along the bottom of the map. No mask
    # ships for it, so: the grey (unsaturated) texels in the lowest eighth
    h = read(os.path.join(SRC, HEAD["bc"] + ".png"), LOW)[..., :3]
    grey = (h.max(-1) - h.min(-1)) < 0.06
    grey[:int(LOW * 0.875)] = False
    paint(HEAD, grow(grey, GROW + 2), "bare_head")
