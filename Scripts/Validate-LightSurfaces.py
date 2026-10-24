"""Проверка реальных компонентов и трассировок шага 2.2; ассеты не сохраняет.
Запуск: py ".../Scripts/Validate-LightSurfaces.py" ".../report.json"
Работает на L_LightPrototype в редакторе и в PIE.
"""
import json
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal


def ref(obj):
    # Путь позволяет отличить сохранённый Actor, его PIE-копию и компонент.
    return obj.get_path_name() if obj else None


def vector(value):
    # Числа сохраняются отдельно для независимой сверки положения и нормали.
    return {axis: float(getattr(value, axis)) for axis in ('x', 'y', 'z')}


def inspect():
    # Канал Python получает проектное имя из загруженного CollisionProfile, не номер TraceTypeQuery.
    channel = unreal.CollisionChannel.ECC_PUZZLE_LIGHT
    trace_channel = unreal.TraceTypeQuery.ECC_PUZZLE_LIGHT
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game = editor.get_game_world()
    world = game or editor.get_editor_world()
    report = {'timestamp_utc': datetime.now(timezone.utc).isoformat(),
              'project': unreal.Paths.get_project_file_path(), 'world': ref(world),
              'pie': bool(game), 'channel': str(channel), 'reflectors': [], 'geometry': [], 'errors': []}
    if 'L_LightPrototype' not in world.get_name():
        raise RuntimeError('Open L_LightPrototype first')

    def check(condition, text):
        # Ошибки собираются вместе, чтобы видно было все несовпадения реальных BP.
        if not condition:
            report['errors'].append(text)

    def trace(start, end, query=trace_channel):
        # Реальный первый блокирующий компонент; никакие устройства не исключаются.
        hit = unreal.SystemLibrary.line_trace_single(world, start, end, query, False, [], unreal.DrawDebugTrace.NONE, ignore_self=False)
        # to_dict использует публичный native-break; внутренние поля HitResult защищены.
        values = hit.to_dict() if hit else {}
        component = values.get('hit_component')
        return component, {'component': ref(component), 'point': vector(values['impact_point']) if hit else None}

    cls = unreal.load_class(None, '/Script/SandsOfSorrow.SandsOfSorrowReflector')
    reflectors = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    check(len(reflectors) == 2, 'Expected two map reflectors')
    for actor in reflectors:
        surface = actor.get_editor_property('working_surface')
        head = actor.get_editor_property('head_mesh')
        base = actor.get_editor_property('base_mesh')
        normal = actor.get_face_normal()
        center = surface.get_world_location()
        front, front_data = trace(center + normal * 20.0, center - normal * 20.0)
        back, back_data = trace(center - normal * 20.0, center + normal * 20.0)
        visible, visibility_data = trace(center + normal * 20.0, center - normal * 20.0, unreal.TraceTypeQuery.ECC_VISIBILITY)
        components = {c.get_name(): {'light': c.get_collision_response_to_channel(channel).value,
            'visibility': c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY).value,
            'pawn': c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN).value,
            'enabled': str(c.get_collision_enabled())} for c in actor.get_components_by_class(unreal.PrimitiveComponent)}
        row = {'actor': ref(actor), 'class': ref(actor.get_class()), 'surface': ref(surface),
               'center': vector(center), 'normal': vector(normal), 'scale_supported': actor.has_supported_light_transform(),
               'index': actor.get_rotation_index(), 'components': components,
               'front': front_data, 'back': back_data, 'visibility': visibility_data}
        report['reflectors'].append(row)
        check(actor.has_supported_light_transform(), 'Unsupported reflector transform: ' + ref(actor))
        check(front == surface, 'Front missed working surface: ' + ref(actor))
        check(back == head, 'Back must hit head: ' + ref(actor))
        check(visible == head, 'Visibility must still hit original head: ' + ref(actor))
        check(surface.get_collision_response_to_channel(channel).value == 2, 'Surface must block light')
        check(head.get_collision_response_to_channel(channel).value == 2, 'Head must block light')
        check(base.get_collision_response_to_channel(channel).value == 2, 'Base must block light')
        check(surface.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY).value == 0, 'Surface must ignore Visibility')
        check(surface.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN).value == 0, 'Surface must ignore Pawn')
    # Обычная геометрия текущей карты должна наследовать Block нового канала.
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        component = actor.static_mesh_component
        response = component.get_collision_response_to_channel(channel).value
        report['geometry'].append({'actor': ref(actor), 'component': ref(component), 'light': response})
        check(response == 2, 'Geometry must block light: ' + ref(actor))
    if game:
        # Проверяем итог после PostInitializeComponents и BP Construction Script.
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = pc.get_controlled_pawn() if pc else None
        check(bool(pawn), 'Missing possessed player')
        report['player'] = {'pawn': ref(pawn), 'components': []}
        if pawn:
            for component in pawn.get_components_by_class(unreal.PrimitiveComponent):
                response = component.get_collision_response_to_channel(channel).value
                report['player']['components'].append({'component': ref(component), 'light': response})
                check(response == 0, 'Player component must ignore light: ' + ref(component))
    report['dirty_content'] = [ref(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    report['dirty_maps'] = [ref(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    return report


output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / 'Verification/LightSurfaces.json'
try:
    result = inspect()
except Exception:
    result = {'errors': [traceback.format_exc()]}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
if result['errors']:
    raise RuntimeError('SANDS_LIGHT_SURFACES_FAILED ' + str(output))
unreal.log('SANDS_LIGHT_SURFACES_OK ' + str(output))
