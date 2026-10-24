"""Проверка сохранённого нерешённого старта световой комнаты в редакторе или PIE.
Запуск: py ".../Scripts/Validate-LightTrace.py" ".../report.json".
Чистый расчёт не поворачивает устройства и не сохраняет ассеты.
"""
import json
import math
import sys
import traceback
from pathlib import Path
import unreal


def ref(obj):
    # Путь различает редакторский Actor и его игровую копию.
    return obj.get_path_name() if obj else None


def vector(value):
    # Координаты сохраняются числами для независимой проверки с допуском.
    return [float(value.x), float(value.y), float(value.z)]


def describe(result):
    # Диагностика пути остаётся отдельно от применённого состояния комнаты.
    return {'termination': str(result.termination), 'reflections': result.reflection_count,
            'distance': result.travelled_distance, 'receiver': ref(result.reached_receiver),
            'primary_traces': result.primary_trace_count, 'offset_traces': result.offset_trace_count,
            'segments': [{'start': vector(s.start), 'end': vector(s.end)} for s in result.segments]}


def inspect():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game = editor.get_game_world()
    world = game or editor.get_editor_world()
    report = {'world': ref(world), 'pie': bool(game), 'blueprints': [], 'errors': []}
    if 'L_LightPrototype' not in world.get_name():
        raise RuntimeError('Open L_LightPrototype first')
    for suffix in ('LightEmitter', 'LightReceiver', 'LightPuzzle'):
        path = '/Game/Blueprints/Logic/Light/BP_' + suffix
        bp = unreal.load_asset(path)
        if not game:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        expected = '/Script/SandsOfSorrow.SandsOfSorrow' + suffix
        report['blueprints'].append({'asset': ref(bp), 'parent': ref(bp.get_blueprint_parent_class()),
                                     'status': str(bp.get_editor_property('status'))})
        if ref(bp.get_blueprint_parent_class()) != expected or bp.get_editor_property('status') != unreal.BlueprintStatus.BS_UP_TO_DATE:
            report['errors'].append('Blueprint parent/compile: ' + path)
    cls = unreal.load_class(None, '/Script/SandsOfSorrow.SandsOfSorrowLightPuzzle')
    puzzles = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    if len(puzzles) != 1:
        raise RuntimeError('Expected one saved puzzle')
    puzzle = puzzles[0]
    emitter = puzzle.get_editor_property('emitter')
    receiver = puzzle.get_editor_property('receiver')
    mirrors = list(puzzle.get_editor_property('reflectors'))
    before = [m.get_rotation_index() for m in mirrors]
    illumination = receiver.get_editor_property('is_illuminated')
    result = puzzle.trace_beam()
    repeat = puzzle.trace_beam()
    report.update(puzzle=ref(puzzle), emitter=ref(emitter), receiver=ref(receiver),
                  reflectors=[ref(m) for m in mirrors], indices=before, result=describe(result),
                  repeat=describe(repeat), illuminated=illumination)
    # В сохранённой комнате оба зеркала стоят за один шаг до правильного положения.
    if len(mirrors) != 2 or len(set(ref(m) for m in mirrors)) != 2 or before != [0, 0]:
        report['errors'].append('Expected two distinct reflectors at initial index 0')
    for mirror in mirrors:
        angle = mirror.get_editor_property('initial_head_rotation').yaw
        if not math.isclose(angle, -45.0, abs_tol=0.01):
            report['errors'].append('Unexpected starting head angle: ' + ref(mirror))
    if result.termination != unreal.LightBeamTermination.BLOCKED or result.reached_receiver or len(result.segments) != 1:
        report['errors'].append('Initial beam must stop at first unturned mirror')
    if report['result'] != report['repeat'] or before != [m.get_rotation_index() for m in mirrors] or illumination != receiver.get_editor_property('is_illuminated'):
        report['errors'].append('Pure trace changed result or device state')
    if result.primary_trace_count > 17 or result.offset_trace_count > 16 or len(result.segments) > 17:
        report['errors'].append('Query/segment guard exceeded')
    if not puzzle.is_actor_tick_enabled() or emitter.is_actor_tick_enabled() or receiver.is_actor_tick_enabled():
        report['errors'].append('Only coordinator may tick')
    report['dirty_content'] = [ref(x) for x in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    report['dirty_maps'] = [ref(x) for x in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    return report


output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / 'Verification/LightTrace.json'
try:
    result = inspect()
except Exception:
    result = {'errors': [traceback.format_exc()]}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
if result['errors']:
    raise RuntimeError('SANDS_LIGHT_TRACE_FAILED ' + str(output))
unreal.log('SANDS_LIGHT_TRACE_OK ' + str(output))
