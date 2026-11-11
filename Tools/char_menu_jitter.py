"""Menu-lean jitter log: in Play, records hands, elbows and spine every frame for SECONDS and
writes Tools/.bridge/jitter_<TAG>.csv, then prints the numbers. Start Play first.

  python3 Tools/ue.py -c "TAG='base'; FPS_CAP=15; exec(open('Tools/char_menu_jitter.py').read())"

FPS_CAP sets t.MaxFPS for the run (0 = uncapped) and puts it back after.
"""
import os
import unreal

TAG = globals().get("TAG", "run")
SECONDS = globals().get("SECONDS", 15.0)
FPS_CAP = globals().get("FPS_CAP", 0)
BONES = ("hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "spine_03")
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                   "Tools", ".bridge", "jitter_%s.csv" % TAG)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
mesh = pawn.get_editor_property("mesh")
wheel = pawn.get_component_by_class(unreal.VisexPoseWheelComponent)
unreal.SystemLibrary.execute_console_command(world, "t.MaxFPS %d" % FPS_CAP)
rows, state = [], {"t": 0.0}


def tick(dt):
    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        unreal.unregister_slate_post_tick_callback(state["h"])
        return
    t = unreal.GameplayStatics.get_time_seconds(world)
    if rows and t == rows[-1][0]:
        return                                   # no new game frame since the last slate tick
    r = [t]
    for b in BONES:
        p = mesh.get_socket_location(b)
        r += [p.x, p.y, p.z]
    r += [wheel.get_editor_property(k) for k in ("hand_fix_zl", "hand_fix_zr", "arm_push_l", "arm_push_r", "hand_gap_l", "hand_gap_r")]
    rows.append(r)
    if t - rows[0][0] >= SECONDS:
        unreal.unregister_slate_post_tick_callback(state["h"])
        unreal.SystemLibrary.execute_console_command(world, "t.MaxFPS 0")
        with open(OUT, "w") as f:
            f.write("t," + ",".join("%s_%s" % (b, a) for b in BONES for a in "xyz") + ",fixl,fixr,pushl,pushr,gapl,gapr\n")
            for r2 in rows:
                f.write(",".join("%.4f" % v for v in r2) + "\n")


state["h"] = unreal.register_slate_post_tick_callback(tick)
print("logging %s for %.0f s at cap %d -> %s" % (TAG, SECONDS, FPS_CAP, OUT))
