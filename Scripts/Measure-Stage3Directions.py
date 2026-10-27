"""Измеряет направления, диагонали, влияние камеры и неполный ввод через Enhanced Input в PIE."""

import builtins
import json
import math
import sys
from datetime import datetime, timezone
from pathlib import Path

import unreal


# Допуски относятся к измерительному сценарию; они не меняют игровые настройки.
SPEED_TOLERANCE = 0.01
DIRECTION_DOT_MIN = 0.999
START = unreal.Vector(-1100.0, -400.0, 100.0)
WARMUP = 0.2
DRIVE = 1.0
SETTLE = 0.25
STAMP = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
# Дополнительный каталог позволяет проверять следующие настройки, сохраняя свидетельства шага 3.2.
OUTPUT_DIR = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / 'Verification/Stage3/20261026_Step2'
OUTPUT = OUTPUT_DIR / ('directions-' + STAMP + '.json')
previous = getattr(builtins, '_sands_stage3_directions', None)
if previous and not previous['done']:
    raise RuntimeError('A direction measurement is already active')


def xyz(value):
    # В отчёте сохраняем числа без адресов временных Python-обёрток UE.
    return [round(float(getattr(value, axis)), 5) for axis in ('x', 'y', 'z')]


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
if not world or 'L_LightPrototype' not in world.get_path_name():
    raise RuntimeError('Start standard PIE on L_LightPrototype')
controller = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = controller.get_controlled_pawn() if controller else None
if not pawn or pawn.get_class().get_path_name() != '/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C':
    raise RuntimeError('Expected possessed gameplay Pawn')
# BlueprintInternalUseOnly не имеет Python-обёртки; вызываем подтверждённый UFUNCTION через reflection.
library = unreal.get_default_object(unreal.load_class(None, '/Script/Engine.SubsystemBlueprintLibrary'))
subsystem = library.call_method('GetLocalPlayerSubSystemFromPlayerController',
                                (controller, unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
if not subsystem:
    raise RuntimeError('Missing local Enhanced Input subsystem for the active Controller')
movement = pawn.get_editor_property('character_movement')
action = pawn.get_editor_property('move_action')
context = controller.get_editor_property('player_mapping_context')
mappings = context.get_editor_property('default_key_mappings').get_editor_property('mappings')
key_mappings = {mapping.get_editor_property('key').export_text(): mapping for mapping in mappings
                if mapping.get_editor_property('action') == action}
if set(key_mappings) != {'W', 'S', 'A', 'D'}:
    raise RuntimeError('Unexpected saved movement mappings')

diagonal = math.sqrt(0.5)
cases = [
    {'name': 'W', 'keys': ['W'], 'expected': [1, 0]},
    {'name': 'S', 'keys': ['S'], 'expected': [-1, 0]},
    {'name': 'A', 'keys': ['A'], 'expected': [0, -1]},
    {'name': 'D', 'keys': ['D'], 'expected': [0, 1]},
    {'name': 'W+A', 'keys': ['W', 'A'], 'expected': [diagonal, -diagonal]},
    {'name': 'W+D', 'keys': ['W', 'D'], 'expected': [diagonal, diagonal]},
    {'name': 'S+A', 'keys': ['S', 'A'], 'expected': [-diagonal, -diagonal]},
    {'name': 'S+D', 'keys': ['S', 'D'], 'expected': [-diagonal, diagonal]},
    {'name': 'W_yaw90', 'keys': ['W'], 'yaw': 90, 'expected': [0, 1]},
    {'name': 'W_yaw180', 'keys': ['W'], 'yaw': 180, 'expected': [-1, 0]},
    {'name': 'W_pitch-55', 'keys': ['W'], 'pitch': -55, 'expected': [1, 0]},
    {'name': 'W_pitch25', 'keys': ['W'], 'pitch': 25, 'expected': [1, 0]},
    {'name': 'D_partial025', 'keys': ['D'], 'magnitude': 0.25, 'expected': [0, 1]},
]
fields = ('max_walk_speed', 'max_acceleration', 'braking_deceleration_walking', 'ground_friction',
          'use_separate_braking_friction', 'braking_friction', 'braking_friction_factor',
          'orient_rotation_to_movement', 'jump_z_velocity', 'air_control')
cdo_movement = unreal.get_default_object(pawn.get_class()).get_editor_property('character_movement')
report = {'timestamp_utc': STAMP, 'project': unreal.Paths.get_project_file_path(),
          'world': world.get_path_name(), 'mode': 'PIE: Enhanced Input injection with saved IMC modifiers/triggers',
          'physical_keyboard_mouse': False, 'start': xyz(START),
          'blueprint_defaults': {n: cdo_movement.get_editor_property(n) for n in fields},
          'runtime_defaults': {n: movement.get_editor_property(n) for n in fields},
          'move_action': action.get_path_name(), 'mapping_context': context.get_path_name(),
          'speed_tolerance': SPEED_TOLERANCE, 'direction_dot_min': DIRECTION_DOT_MIN,
          'cases': [], 'errors': []}
if report['runtime_defaults']['max_walk_speed'] != 500.0 or report['runtime_defaults'] != report['blueprint_defaults']:
    raise RuntimeError('Runtime movement settings differ from the 500 cm/s gameplay Blueprint')
state = {'handle': None, 'done': False, 'case_index': -1, 'case_started': 0.0, 'samples': [], 'output': str(OUTPUT)}


def write_report():
    # Каждый запуск имеет отдельный файл: неудачные результаты не затираются повтором.
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2), encoding='utf-8')


def finish():
    # Инъекция покадровая и сама прекращается; callback снимается до записи результата.
    unreal.unregister_slate_post_tick_callback(state['handle'])
    state['done'] = True
    write_report()
    unreal.log('SANDS_STAGE3_DIRECTIONS_' + ('FAILED ' if report['errors'] else 'OK ') + str(OUTPUT))


def begin_case(now):
    # Телепорт используется только для исходной точки между независимыми измерениями.
    state['case_index'] += 1
    if state['case_index'] == len(cases):
        finish()
        return
    case = cases[state['case_index']]
    movement.stop_movement_immediately()
    pawn.consume_movement_input_vector()
    pawn.set_actor_location(START, sweep=False, teleport=True)
    controller.set_control_rotation(unreal.Rotator(pitch=case.get('pitch', 0), yaw=case.get('yaw', 0), roll=0))
    state['case_started'] = now
    state['samples'] = []


def complete_case(case):
    # Проверяем фактическую скорость и направление после разгона, а также длину ввода из Move.
    steady = [s for s in state['samples'] if WARMUP + 0.6 <= s['t'] <= WARMUP + 0.95]
    if len(steady) < 3:
        raise RuntimeError('Too few steady samples for ' + case['name'])
    magnitude = case.get('magnitude', 1.0)
    expected_speed = 500.0 * magnitude
    speeds = [s['speed'] for s in steady]
    dots = [(s['velocity'][0] * case['expected'][0] + s['velocity'][1] * case['expected'][1]) / s['speed']
            if s['speed'] > 0 else 0 for s in steady]
    input_lengths = [math.sqrt(sum(v * v for v in s['last_input'])) for s in steady]
    row = dict(case, expected_speed=expected_speed, steady_min=min(speeds), steady_max=max(speeds),
               steady_mean=sum(speeds) / len(speeds), direction_dot_min=min(dots),
               input_length_min=min(input_lengths), input_length_max=max(input_lengths),
               control_rotation=steady[-1]['control_rotation'], samples=state['samples'])
    row['pass'] = (all(abs(v - expected_speed) <= expected_speed * SPEED_TOLERANCE for v in speeds)
                   and min(dots) >= DIRECTION_DOT_MIN
                   and all(abs(v - magnitude) < 0.01 for v in input_lengths)
                   and all(abs(s['velocity'][2]) < 0.1 for s in steady))
    if not row['pass']:
        report['errors'].append('Speed/direction/input magnitude failed: ' + case['name'])
    report['cases'].append(row)
    write_report()


def measure_tick(delta_seconds):
    # Используется игровое время; закрытие PIE обрывает сценарий с явной ошибкой.
    if editor.get_game_world() != world:
        raise RuntimeError('PIE ended during measurement')
    now = unreal.GameplayStatics.get_time_seconds(world)
    if state['case_index'] < 0:
        begin_case(now)
        return
    case = cases[state['case_index']]
    elapsed = now - state['case_started']
    velocity = pawn.get_velocity()
    rotation = controller.get_control_rotation()
    state['samples'].append({'t': round(elapsed, 5), 'dt_slate': round(float(delta_seconds), 6),
                             'location': xyz(pawn.get_actor_location()), 'velocity': xyz(velocity),
                             'speed': math.hypot(velocity.x, velocity.y),
                             'last_input': xyz(pawn.get_last_movement_input_vector()),
                             'control_rotation': [rotation.pitch, rotation.yaw, rotation.roll]})
    if WARMUP <= elapsed < WARMUP + DRIVE:
        for key in case['keys']:
            mapping = key_mappings[key]
            subsystem.inject_input_vector_for_action(action, unreal.Vector(case.get('magnitude', 1.0), 0, 0),
                mapping.get_editor_property('modifiers'), mapping.get_editor_property('triggers'))
    if elapsed >= WARMUP + DRIVE + SETTLE:
        complete_case(case)
        begin_case(now)


def tick(delta_seconds):
    # Даже ошибка API снимает callback и оставляет отчёт с причиной вместо бесконечного лога.
    try:
        measure_tick(delta_seconds)
    except Exception as error:
        report['errors'].append(str(error))
        finish()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
builtins._sands_stage3_directions = state
unreal.log('SANDS_STAGE3_DIRECTIONS_STARTED ' + str(OUTPUT))
