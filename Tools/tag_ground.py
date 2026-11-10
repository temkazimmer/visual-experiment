# Tags the walkable ground in the open level with "VisexGround", so photo mode's scene strip
# (AVisexMenuDirector::SetSceneStripped) keeps it in Shipping, where actor labels don't exist.
#   python3 Tools/ue.py Tools/tag_ground.py      (with Play stopped; saves the level)
import unreal

TAG = unreal.Name("VisexGround")
subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tagged = []
for a in subsys.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    label = a.get_actor_label()
    if label.startswith(("Dunes", "LowerGround", "StepRamp")) or label == "Plaza":
        tags = list(a.tags)
        if TAG not in tags:
            tags.append(TAG)
            a.set_editor_property("tags", tags)
        tagged.append(label)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print("tagged", len(tagged), sorted(tagged))
