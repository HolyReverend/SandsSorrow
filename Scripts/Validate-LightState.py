"""Проверка нерешённого начального состояния комнаты в редакторе и PIE."""
import json
import sys
import traceback
from pathlib import Path
import unreal


def ref(obj):
    # Путь отличает сохранённый мир от игровой копии.
    return obj.get_path_name() if obj else None


def inspect():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game = editor.get_game_world()
    world = game or editor.get_editor_world()
    cls = unreal.load_class(None, '/Script/SandsOfSorrow.SandsOfSorrowLightPuzzle')
    puzzles = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    report = {'world': ref(world), 'pie': bool(game), 'errors': [], 'puzzles': len(puzzles)}
    if len(puzzles) != 1:
        report['errors'].append('Expected one puzzle')
        return report
    puzzle = puzzles[0]
    receiver = puzzle.get_editor_property('receiver')
    emitter = puzzle.get_editor_property('emitter')
    door = puzzle.get_editor_property('door')
    reward = list(puzzle.get_editor_property('reward_lights'))
    pure = puzzle.trace_beam()
    applied = puzzle.get_editor_property('last_trace_result')
    report.update(puzzle=ref(puzzle), receiver=ref(receiver), door=ref(door), reward=[ref(x) for x in reward],
                  solved=puzzle.get_editor_property('is_solved'), illuminated=receiver.get_editor_property('is_illuminated'),
                  pure_termination=str(pure.termination), applied_termination=str(applied.termination),
                  door_open=door.is_fully_open() if door else None,
                  reward_visible=[x.get_editor_property('point_light_component').is_visible() for x in reward],
                  puzzle_tick=puzzle.is_actor_tick_enabled(), receiver_tick=receiver.is_actor_tick_enabled(),
                  emitter_tick=emitter.is_actor_tick_enabled())
    # Ни подсветка, ни дверь не могут сработать до поворота обоих зеркал.
    if pure.termination != unreal.LightBeamTermination.BLOCKED or puzzle.get_editor_property('is_solved') or receiver.get_editor_property('is_illuminated'):
        report['errors'].append('Saved room must begin unsolved')
    if not door or door.is_fully_open() or len(reward) != 1 or any(report['reward_visible']):
        report['errors'].append('Initial door/reward state is incorrect')
    if game and applied.termination != unreal.LightBeamTermination.BLOCKED:
        report['errors'].append('Automatic PIE pass did not apply blocked beam')
    if game and not puzzle.is_actor_tick_enabled():
        report['errors'].append('Coordinator is not ticking in PIE')
    if receiver.is_actor_tick_enabled() or emitter.is_actor_tick_enabled():
        report['errors'].append('Devices should not tick')
    report['dirty_content'] = [ref(x) for x in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    report['dirty_maps'] = [ref(x) for x in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    return report


output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / 'Verification/LightState.json'
try:
    result = inspect()
except Exception:
    result = {'errors': [traceback.format_exc()]}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
if result['errors']:
    raise RuntimeError('SANDS_LIGHT_STATE_FAILED ' + str(output))
unreal.log('SANDS_LIGHT_STATE_OK ' + str(output))
