"""Измеряет геометрию и фактические компоненты персонажа; не меняет ассеты.
Запуск в PIE: py ".../Measure-CharacterScale.py" ".../before.json"
Рост калибруется по вершинам LOD0 в опорной позе, отдельно от анимированной позы и капсулы.
"""
import json
import sys
from pathlib import Path
import unreal


# Числовые значения сохраняются без адресов временных обёрток Unreal.
def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def bounds(dynamic):
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(dynamic)
    return {'min': xyz(box.min), 'max': xyz(box.max), 'height_cm': box.max.z - box.min.z}


# Извлечение геометрии не сохраняет и не модифицирует исходный SkeletalMesh.
def inspect():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if not world or 'L_LightPrototype' not in world.get_path_name():
        raise RuntimeError('Start ordinary PIE on L_LightPrototype')
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = pc.get_controlled_pawn()
    mesh = pawn.get_editor_property('mesh')
    asset = mesh.get_editor_property('skeletal_mesh_asset')
    cap = pawn.get_editor_property('capsule_component')
    boom = pawn.get_editor_property('camera_boom')
    camera = pawn.get_editor_property('follow_camera')
    manager = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    movement = pawn.get_editor_property('character_movement')
    ref = unreal.DynamicMesh()
    _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        asset, ref, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Reference geometry extraction failed')
    posed = unreal.DynamicMesh()
    _, transform, outcome = unreal.GeometryScript_SceneUtils.copy_mesh_from_component(
        mesh, posed, unreal.GeometryScriptCopyMeshFromComponentOptions(), True)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError('Posed geometry extraction failed')
    ref_box = bounds(ref)
    posed_box = bounds(posed)
    rotation = pc.get_control_rotation()
    camera_rotation = manager.get_camera_rotation()
    cap_half = cap.get_editor_property('capsule_half_height')
    return {'project': unreal.Paths.get_project_file_path(), 'world': world.get_path_name(),
        'pawn_class': pawn.get_class().get_path_name(), 'controller_class': pc.get_class().get_path_name(),
        'camera_manager_class': manager.get_class().get_path_name(),
        'mesh_asset': asset.get_path_name(), 'anim_class': mesh.get_editor_property('anim_class').get_path_name(),
        'actor_scale': xyz(pawn.get_actor_scale3d()), 'actor_location': xyz(pawn.get_actor_location()),
        'mesh_scale': xyz(mesh.get_editor_property('relative_scale3d')), 'mesh_location': xyz(mesh.get_editor_property('relative_location')),
        'reference_bounds': ref_box, 'standing_reference_height_cm': ref_box['height_cm']*mesh.get_editor_property('relative_scale3d').z*pawn.get_actor_scale3d().z,
        'posed_world_bounds': posed_box, 'feet_world_cm': posed_box['min'][2],
        'capsule_radius': cap.get_editor_property('capsule_radius'), 'capsule_half_height': cap_half,
        'capsule_bottom_cm': pawn.get_actor_location().z-cap_half,
        'camera': {'arm': boom.get_editor_property('target_arm_length'), 'target_offset': xyz(boom.get_editor_property('target_offset')),
            'socket_offset': xyz(boom.get_editor_property('socket_offset')), 'probe': boom.get_editor_property('probe_size'),
            'collision': boom.get_editor_property('do_collision_test'), 'fov': camera.get_editor_property('field_of_view'),
            'position': xyz(manager.get_camera_location()), 'rotation': [camera_rotation.pitch,camera_rotation.yaw,camera_rotation.roll],
            'control_rotation': [rotation.pitch,rotation.yaw,rotation.roll], 'look_sensitivity': str(pawn.get_editor_property('look_sensitivity'))},
        'movement': {name: movement.get_editor_property(name) for name in ('max_walk_speed','max_acceleration','braking_deceleration_walking','max_step_height','walkable_floor_angle','jump_z_velocity','air_control')},
        'bones': {name: xyz(mesh.get_socket_location(name)) for name in ('root','pelvis','head','foot_l','foot_r','ball_l','ball_r')},
        'actors': [{'name': a.get_name(), 'label': a.get_actor_label(), 'location': xyz(a.get_actor_location()), 'bounds': [xyz(v) for v in a.get_actor_bounds(False)]}
                   for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if any(s in a.get_actor_label().lower() for s in ('wall','platform','step','door','reflector','floor','corner'))]}


# Путь задаётся явно, чтобы очередной замер не затирал исходное сравнение.
output = Path(sys.argv[1])
output.parent.mkdir(parents=True, exist_ok=True)
result = inspect()
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('SANDS_CHARACTER_SCALE_MEASURED ' + str(output))
