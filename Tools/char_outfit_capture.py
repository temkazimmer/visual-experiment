"""Close shots of the dressed character at the menu spot (editor world, nothing saved)."""
import os, shutil, unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
stand = by["MenuModel"]
root = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")
src = open(os.path.join(root, "capture.py")).read()
bp = unreal.load_asset("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter")
hh = unreal.get_default_object(bp.generated_class()).get_editor_property("capsule_component").get_unscaled_capsule_half_height()
loc = stand.get_actor_location() + unreal.Vector(0, 0, hh)
char = eas.spawn_actor_from_class(bp.generated_class(), loc, unreal.Rotator(roll=0, pitch=0, yaw=stand.get_actor_rotation().yaw + 90.0))
comps = char.get_components_by_class(unreal.SkeletalMeshComponent)
print([(c.get_name(), c.get_class().get_name(), c.get_skeletal_mesh_asset().get_name() if c.get_skeletal_mesh_asset() else None) for c in comps])
body = [c for c in comps if c.get_class().get_name() != "VisexShellComponent"][0]
body.override_animation_data(unreal.load_asset(globals().get("POSE", "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle")))
stand.set_is_temporarily_hidden_in_editor(True)
f = char.get_actor_forward_vector(); r = char.get_actor_right_vector()
feet = loc - unreal.Vector(0, 0, hh)
def P(v): return (v.x, v.y, v.z)
SH = globals().get("SHOTS") or {
    "full":  (feet + f * 380 + r * 150 + unreal.Vector(0, 0, 95), feet + unreal.Vector(0, 0, 82), 30.0),
    "torso": (feet + f * 190 + r * 110 + unreal.Vector(0, 0, 120), feet + unreal.Vector(0, 0, 112), 30.0),
    "hip":   (feet + f * 130 + r * 150 + unreal.Vector(0, 0, 85), feet + unreal.Vector(0, 0, 75), 30.0),
    "back":  (feet - f * 330 + r * 200 + unreal.Vector(0, 0, 95), feet + unreal.Vector(0, 0, 82), 30.0),
}
try:
    for name, (c, t, fov) in SH.items():
        g = {"__name__": "__bridge__", "VIEW": (P(c), P(t), fov)}
        exec(compile(src, "capture.py", "exec"), g)
        shutil.move(os.path.join(root, ".bridge", "capture.png"), os.path.join(root, ".bridge", "fit_%s.png" % name))
finally:
    eas.destroy_actor(char)
    stand.set_is_temporarily_hidden_in_editor(False)
