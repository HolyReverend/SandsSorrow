"""Inspect the current Third Person baseline without saving assets. Run in Unreal's Python console."""
import json
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal


def ref(obj):
    return obj.get_path_name() if obj else None


def vector(value):
    return {key: float(getattr(value, key)) for key in ("x", "y", "z")}


def inspect_baseline():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game_world = editor.get_game_world()
    editor_world = None if game_world else editor.get_editor_world()
    result = {"timestamp_utc": datetime.now(timezone.utc).isoformat(),
              "project": unreal.Paths.get_project_file_path(),
              "engine": unreal.SystemLibrary.get_engine_version(),
              "editor_world": ref(editor_world), "pie": bool(game_world), "errors": []}
    if game_world:
        pc = unreal.GameplayStatics.get_player_controller(game_world, 0)
        pawn = pc.get_controlled_pawn() if pc else None
        gm = unreal.GameplayStatics.get_game_mode(game_world)
        result["runtime"] = {"world": ref(game_world), "game_mode": ref(gm),
            "game_mode_class": ref(gm.get_class()) if gm else None,
            "controller": ref(pc), "controller_class": ref(pc.get_class()) if pc else None,
            "pawn": ref(pawn), "pawn_class": ref(pawn.get_class()) if pawn else None,
            "possessed": bool(pawn and pawn.get_controller() == pc),
            "location": vector(pawn.get_actor_location()) if pawn else None,
            "velocity": vector(pawn.get_velocity()) if pawn else None,
            "camera_location": vector(unreal.GameplayStatics.get_player_camera_manager(game_world, 0).get_camera_location()) if pc else None}
        if not (gm and pc and pawn and result["runtime"]["possessed"]):
            result["errors"].append("Missing GameMode/controller/possessed Pawn")
    else:
        result["dirty_packages_before"] = [ref(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
        result["actors"] = len(actors)
        result["player_starts"] = [{"path": ref(a), "location": vector(a.get_actor_location())} for a in starts]
        gm_class = editor_world.get_world_settings().get_editor_property("default_game_mode")
        result["map_game_mode"] = ref(gm_class)
        if not gm_class:
            raise RuntimeError("The baseline map must specify its Third Person GameMode")
        gm_cdo = unreal.get_default_object(gm_class)
        pawn_class = gm_cdo.get_editor_property("default_pawn_class")
        pc_class = gm_cdo.get_editor_property("player_controller_class")
        pawn_cdo = unreal.get_default_object(pawn_class)
        pc_cdo = unreal.get_default_object(pc_class)
        mesh = pawn_cdo.get_editor_property("mesh")
        anim_class = mesh.get_editor_property("anim_class")
        result["chain"] = {"game_mode": ref(gm_class), "pawn": ref(pawn_class),
            "controller": ref(pc_class), "mesh": ref(mesh.get_skeletal_mesh_asset()), "anim_class": ref(anim_class)}
        result["actions"] = {name: ref(pawn_cdo.get_editor_property(name)) for name in
                             ("jump_action", "move_action", "look_action", "mouse_look_action")}
        result["input_contexts"] = []
        contexts = list(pc_cdo.get_editor_property("default_mapping_contexts")) + list(pc_cdo.get_editor_property("mobile_excluded_mapping_contexts"))
        for context in contexts:
            mappings = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
            result["input_contexts"].append({"asset": ref(context), "mappings": [
                {"key": str(unreal.InputLibrary.key_get_display_name(m.get_editor_property("key"))), "action": ref(m.get_editor_property("action"))} for m in mappings]})
        result["blueprints"] = []
        for cls in (gm_class, pawn_class, pc_class, anim_class):
            bp = unreal.load_asset(ref(cls).removesuffix("_C"))
            if not isinstance(bp, unreal.Blueprint):
                raise RuntimeError("Expected Blueprint: " + str(ref(cls)))
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            status = bp.get_editor_property("status")
            result["blueprints"].append({"asset": ref(bp), "status": str(status)})
            if status != unreal.BlueprintStatus.BS_UP_TO_DATE:
                result["errors"].append("Blueprint compile status: " + ref(bp) + " " + str(status))
        result["dirty_packages_after"] = [ref(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
        if not starts or any(not value for value in result["chain"].values()) or any(not value for value in result["actions"].values()):
            result["errors"].append("Missing PlayerStart or required baseline resource")
    return result


output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / "Baseline" / (datetime.now().strftime("%Y-%m-%d_%H%M%S") + ".json")
try:
    report = inspect_baseline()
except Exception:
    report = {"errors": [traceback.format_exc()]}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
if report["errors"]:
    raise RuntimeError("Baseline validation failed: " + str(output))
unreal.log("SANDS_BASELINE_OK " + str(output))
