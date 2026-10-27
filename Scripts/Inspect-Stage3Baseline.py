"""Снимает исходные параметры движения и геометрию маршрута без сохранения UE-ассетов."""

import json
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal


MAP = "/Game/Maps/TestMaps/L_LightPrototype"
CHARACTER = "/Game/Blueprints/Characters/BP_PlayerCharacter"
CONTROLLER = "/Game/Blueprints/Characters/BP_PlayerController"
ANIM = "/Game/Template/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"
BLEND = "/Game/Template/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run"


def ref(value):
    return value.get_path_name() if value else None


def xyz(value):
    return [round(float(getattr(value, axis)), 3) for axis in ("x", "y", "z")]


def prop(obj, name):
    # Отсутствующее в конкретной версии UE свойство остаётся явным пробелом отчёта.
    try:
        value = obj.get_editor_property(name)
        if isinstance(value, unreal.Object):
            return ref(value)
        if isinstance(value, (bool, int, float, str)):
            return value
        return str(value)
    except Exception as error:
        return {"unavailable": str(error)}


def modifier_row(modifier):
    # Порядок Swizzle и оси Negate определяют фактический знак WASD, а не только их классы.
    row = {"class": ref(modifier.get_class())}
    if isinstance(modifier, unreal.InputModifierSwizzleAxis):
        row["order"] = str(modifier.get_editor_property("order"))
    if isinstance(modifier, unreal.InputModifierNegate):
        row["axes"] = {axis: bool(modifier.get_editor_property(axis)) for axis in ("x", "y", "z")}
    return row


def movement_row(pawn):
    # Читаем компонент экземпляра или CDO: кодовые defaults могут отличаться от сохранённых BP override.
    movement = pawn.get_editor_property("character_movement")
    capsule = pawn.get_editor_property("capsule_component")
    mesh = pawn.get_editor_property("mesh")
    fields = ("max_walk_speed", "max_acceleration", "braking_deceleration_walking",
              "ground_friction", "use_separate_braking_friction", "braking_friction",
              "braking_friction_factor", "orient_rotation_to_movement", "use_controller_desired_rotation",
              "rotation_rate", "jump_z_velocity", "air_control")
    return {
        "class": ref(pawn.get_class()),
        "movement": {name: prop(movement, name) for name in fields},
        "capsule": {"radius": capsule.get_unscaled_capsule_radius(),
                    "half_height": capsule.get_unscaled_capsule_half_height(),
                    "scale": prop(capsule, "relative_scale3d")},
        "mesh": {"asset": ref(mesh.get_skeletal_mesh_asset()), "anim_class": ref(mesh.get_editor_property("anim_class")),
                 "relative_location": prop(mesh, "relative_location"),
                 "relative_rotation": prop(mesh, "relative_rotation"),
                 "relative_scale": prop(mesh, "relative_scale3d")},
        "input": {name: prop(pawn, name) for name in ("move_action", "look_action", "jump_action", "interact_action")},
        "controller_yaw": prop(pawn, "use_controller_rotation_yaw"),
    }


def inspect():
    report = {"timestamp_utc": datetime.now(timezone.utc).isoformat(),
              "project": unreal.Paths.get_project_file_path(),
              "engine": unreal.SystemLibrary.get_engine_version(), "errors": []}
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if ref(editor.get_editor_world()) != MAP + ".L_LightPrototype":
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
    world = editor.get_editor_world()
    report["world"] = ref(world)
    if report["world"] != MAP + ".L_LightPrototype":
        report["errors"].append("Expected L_LightPrototype")

    character_bp = unreal.load_asset(CHARACTER)
    controller_bp = unreal.load_asset(CONTROLLER)
    anim_bp = unreal.load_asset(ANIM)
    blend = unreal.load_asset(BLEND)
    report["blueprints"] = [{"asset": ref(bp), "parent": ref(bp.get_blueprint_parent_class()),
                             "status": str(bp.get_editor_property("status"))}
                            for bp in (character_bp, controller_bp, anim_bp)]
    pawn = unreal.get_default_object(character_bp.generated_class())
    controller = unreal.get_default_object(controller_bp.generated_class())
    report["blueprint_defaults"] = movement_row(pawn)
    report["controller_mapping_context"] = prop(controller, "player_mapping_context")
    context = controller.get_editor_property("player_mapping_context")
    mappings = context.get_editor_property("default_key_mappings").get_editor_property("mappings")
    report["mappings"] = [{"key": mapping.get_editor_property("key").export_text(),
                           "action": ref(mapping.get_editor_property("action")),
                           "modifiers": [modifier_row(modifier) for modifier in mapping.get_editor_property("modifiers")]}
                          for mapping in mappings]
    action = pawn.get_editor_property("move_action")
    report["move_action_type"] = str(action.get_editor_property("value_type"))

    # Сопоставляем полный направленный набор с фактическими ссылками AnimBP/BlendSpace.
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                                    include_soft_package_references=True)
    report["animation"] = {"anim_bp": ref(anim_bp), "blend_space": ref(blend),
        "blend_referencers": [str(item) for item in unreal.EditorAssetLibrary.find_package_referencers_for_asset(BLEND)],
        "anim_dependencies": [str(item) for item in registry.get_dependencies(ANIM, options)],
        "blend_dependencies": [str(item) for item in registry.get_dependencies(BLEND, options)],
        "blend_axes": [{"name": axis.get_editor_property("display_name"),
                        "min": axis.get_editor_property("min"), "max": axis.get_editor_property("max")}
                       for axis in blend.get_editor_property("blend_parameters")],
        "blend_samples": [{"animation": ref(sample.get_editor_property("animation")),
                           "value": xyz(sample.get_editor_property("sample_value"))}
                          for sample in blend.get_editor_property("sample_data")]}

    actor_rows = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        origin, extent = actor.get_actor_bounds(only_colliding_components=False)
        actor_rows.append({"name": actor.get_actor_label(), "class": ref(actor.get_class()),
                           "location": xyz(actor.get_actor_location()), "bounds_origin": xyz(origin),
                           "bounds_extent": xyz(extent)})
    report["actors"] = sorted(actor_rows, key=lambda row: row["name"])
    # Сохранённая комната должна оставаться нерешённой после подготовки маршрута.
    puzzle_class = unreal.load_class(None, "/Script/SandsOfSorrow.SandsOfSorrowLightPuzzle")
    puzzles = unreal.GameplayStatics.get_all_actors_of_class(world, puzzle_class)
    if len(puzzles) != 1:
        report["errors"].append("Expected one light puzzle")
    else:
        puzzle = puzzles[0]
        receiver = puzzle.get_editor_property("receiver")
        door = puzzle.get_editor_property("door")
        rewards = list(puzzle.get_editor_property("reward_lights"))
        termination = puzzle.trace_beam().termination
        report["room"] = {"solved": puzzle.get_editor_property("is_solved"),
                          "receiver_illuminated": receiver.get_editor_property("is_illuminated"),
                          "trace_termination": str(termination),
                          "door_open": door.is_fully_open() if door else None,
                          "reward_visible": [light.get_editor_property("point_light_component").is_visible()
                                             for light in rewards]}
        if (report["room"]["solved"] or report["room"]["receiver_illuminated"] or
                termination != unreal.LightBeamTermination.BLOCKED or
                report["room"]["door_open"] or any(report["room"]["reward_visible"])):
            report["errors"].append("Saved light room does not begin unsolved")
    report["dirty_content_packages"] = [ref(package) for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    report["dirty_map_packages"] = [ref(package) for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    return report


output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / "Verification/Stage3/20261026_Step1/baseline.json"
try:
    result = inspect()
except Exception:
    result = {"errors": [traceback.format_exc()]}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
if result["errors"]:
    raise RuntimeError("SANDS_STAGE3_BASELINE_FAILED " + str(output))
unreal.log("SANDS_STAGE3_BASELINE_OK " + str(output))
