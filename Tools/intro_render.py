"""Render LS_Intro with Movie Render Queue (in-editor PIE executor).

  python3 Tools/ue.py Tools/intro_render.py                 # preview: 1280x720, 4 samples
  python3 Tools/ue.py -c "QUALITY='final'; exec(open('Tools/intro_render.py').read())"

Frames land in Saved/MovieRenders/Intro_<quality>/ as PNG (final: 3840x2400 PNG, 16:10 for Mac screens,
10 temporal samples, motion blur on). Pre-roll: the sequence starts 2 s before the film so
she is mid-stride; the camera cut starts at 2 s and MRQ runs the frames before it as warm-up.
"""
import os
import unreal

QUALITY = globals().get("QUALITY", "preview")
FPS, PRE, END = 24, 2.0, 13.5
FIRST, LAST = int(PRE * FPS), int(END * FPS)
ONLY = globals().get("ONLY")            # (first, last) frame override for quick checks

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
out_dir = os.path.join(root, "Saved", "MovieRenders", "Intro_" + QUALITY)
os.makedirs(out_dir, exist_ok=True)
for f in os.listdir(out_dir):
    if f.endswith(".png"):
        os.remove(os.path.join(out_dir, f))

qs = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
queue = qs.get_queue()
for j in list(queue.get_jobs()):
    queue.delete_job(j)
job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
job.set_editor_property("job_name", "Intro_" + QUALITY)
job.set_editor_property("sequence", unreal.SoftObjectPath("/Game/VisEx/Intro/LS_Intro.LS_Intro"))
job.set_editor_property("map", unreal.SoftObjectPath("/Game/VisEx/Lvl_Dust.Lvl_Dust"))
cfg = job.get_configuration()

o = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
o.set_editor_property("output_directory", unreal.DirectoryPath(out_dir))
o.set_editor_property("file_name_format", "intro.{frame_number}")
o.set_editor_property("output_resolution", unreal.IntPoint(3840, 2400) if QUALITY == "final" else unreal.IntPoint(1280, 800))   # 16:10
if ONLY:
    o.set_editor_property("use_custom_playback_range", True)
    o.set_editor_property("custom_start_frame", ONLY[0])
    o.set_editor_property("custom_end_frame", ONLY[1])
o.set_editor_property("override_existing_output", True)

aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
aa.set_editor_property("spatial_sample_count", 1)
aa.set_editor_property("temporal_sample_count", 10 if QUALITY == "final" else 4)   # 10 at 4K: ~1 h for the film
aa.set_editor_property("override_anti_aliasing", True)
aa.set_editor_property("anti_aliasing_method", unreal.AntiAliasingMethod.AAM_TEMPORAL_AA if QUALITY != "final" else unreal.AntiAliasingMethod.AAM_NONE)
aa.set_editor_property("use_camera_cut_for_warm_up", True)   # the 2 s before the cut
aa.set_editor_property("render_warm_up_frames", True)        # ...run through the renderer too

cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
cv = cfg.find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting)
cv.add_or_update_console_variable("r.MotionBlurQuality", 4)

executor = qs.render_queue_with_executor(unreal.MoviePipelinePIEExecutor)
print("render started", QUALITY, out_dir)
