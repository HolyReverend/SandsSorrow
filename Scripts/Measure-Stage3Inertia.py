"""Сравнивает разгон, отпускание, разворот, поворот и прыжок в обычном PIE через Enhanced Input."""

import builtins
import json
import math
from datetime import datetime, timezone
from pathlib import Path
import unreal


OUT = Path(unreal.Paths.project_saved_dir()) / 'Verification/Stage3/20261026_Step3'
STAMP = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
OUTPUT = OUT / ('inertia-' + STAMP + '.json')
STOP_SPEED = 1.0
WARMUP = 0.2
START = unreal.Vector(-1400, -250, 100)
previous = getattr(builtins, '_sands_stage3_inertia', None)
if previous and not previous['done']:
    raise RuntimeError('An inertia measurement is already active')
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
if not world or 'L_LightPrototype' not in world.get_path_name():
    raise RuntimeError('Start standard PIE on L_LightPrototype')
controller = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = controller.get_controlled_pawn() if controller else None
if not pawn or pawn.get_class().get_path_name() != '/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C':
    raise RuntimeError('Expected possessed gameplay Pawn')
movement = pawn.get_editor_property('character_movement')
library = unreal.get_default_object(unreal.load_class(None, '/Script/Engine.SubsystemBlueprintLibrary'))
subsystem = library.call_method('GetLocalPlayerSubSystemFromPlayerController',
                                (controller, unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
if not subsystem:
    raise RuntimeError('Missing local Enhanced Input subsystem')
move_action = pawn.get_editor_property('move_action')
jump_action = pawn.get_editor_property('jump_action')
context = controller.get_editor_property('player_mapping_context')
mappings = context.get_editor_property('default_key_mappings').get_editor_property('mappings')
keys = {m.get_editor_property('key').export_text(): m for m in mappings}
fields = tuple(json.loads((OUT / 'initial.json').read_text(encoding='utf-8'))['before'])
original = {n: movement.get_editor_property(n) for n in fields}
profiles = json.loads((OUT / 'profiles.json').read_text(encoding='utf-8'))
allowed = {'max_acceleration', 'braking_deceleration_walking', 'ground_friction',
           'braking_friction', 'braking_friction_factor'}
for profile in profiles:
    if set(profile['overrides']) - allowed:
        raise RuntimeError('Profile exceeds ground tuning scope')
    for key, value in profile['overrides'].items():
        if not math.isfinite(value) or value < 0:
            raise RuntimeError('Invalid tuning value: ' + key)
cases = [{'name': 'release', 'drive': 1.0, 'after': 1.0},
         {'name': 'reverse', 'drive': 1.0, 'after': 0.8},
         {'name': 'turn90', 'drive': 1.0, 'after': 0.8},
         {'name': 'air_release', 'drive': 0.8, 'after': 1.5}]
jobs = [(profile, case) for profile in profiles for case in cases]
report = {'timestamp_utc': STAMP, 'world': world.get_path_name(),
          'mode': 'rendered PIE, Enhanced Input with saved mapping modifiers; programmatic input',
          'original_runtime': original, 'start': [START.x, START.y, START.z],
          'stop_threshold_cm_s': STOP_SPEED, 'warmup_s': WARMUP, 'jobs': [], 'errors': []}
saved_transform = pawn.get_actor_transform()
saved_rotation = controller.get_control_rotation()
state = {'handle': None, 'done': False, 'index': -1, 'start_t': 0, 'samples': [], 'output': str(OUTPUT)}


def xyz(vector):
    return [float(getattr(vector, axis)) for axis in ('x', 'y', 'z')]


def persist():
    # Отдельный файл каждого запуска сохраняет как результаты, так и явные неудачи.
    OUT.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, indent=2), encoding='utf-8')


def finish():
    # Настройки экспериментальных профилей действуют только в PIE и всегда восстанавливаются.
    unreal.unregister_slate_post_tick_callback(state['handle'])
    state['done'] = True
    if editor.get_game_world() == world:
        movement.stop_movement_immediately()
        pawn.stop_jumping()
        for name, value in original.items():
            if movement.get_editor_property(name) != value:
                movement.set_editor_property(name, value)
        pawn.set_actor_transform(saved_transform, sweep=False, teleport=True)
        controller.set_control_rotation(saved_rotation)
    persist()
    unreal.log('SANDS_STAGE3_INERTIA_' + ('FAILED ' if report['errors'] else 'OK ') + str(OUTPUT))


def begin(now):
    # Сброс скорости и позиции задаёт одинаковые условия, не участвуя в самой симуляции движения.
    state['index'] += 1
    if state['index'] == len(jobs):
        finish()
        return
    profile, case = jobs[state['index']]
    movement.stop_movement_immediately()
    pawn.stop_jumping()
    pawn.consume_movement_input_vector()
    pawn.set_actor_location(START, sweep=False, teleport=True)
    controller.set_control_rotation(unreal.Rotator(pitch=0, yaw=0, roll=0))
    for name, value in original.items():
        if movement.get_editor_property(name) != value:
            movement.set_editor_property(name, value)
    for name, value in profile['overrides'].items():
        movement.set_editor_property(name, value)
    state['values'] = {n: movement.get_editor_property(n) for n in fields}
    state['start_t'] = now
    state['samples'] = []


def summarize(profile, case):
    # Метрики используют наблюдённые кадры; полные кривые остаются в JSON для перепроверки границ.
    samples = state['samples']
    dt = sorted(s['dt_slate'] for s in samples)
    if len(samples) < 30 or dt[len(dt) // 2] > 0.05:
        raise RuntimeError('Insufficient frame sampling; keep the editor visible and foreground')
    switch = WARMUP + case['drive']
    t95 = next((s['t'] - WARMUP for s in samples if s['t'] >= WARMUP and s['speed'] >= 475), None)
    before = [s for s in samples if s['t'] <= switch]
    after = [s for s in samples if s['t'] > switch]
    row = {'profile': profile['name'], 'case': case['name'], 'settings': state['values'],
           'time_to_95_s': t95, 'command_switch_s': switch,
           'median_dt_s': dt[len(dt) // 2], 'samples': samples}
    if not before or t95 is None:
        raise RuntimeError('Did not reach steady speed before switch')
    reference = before[-1]
    if case['name'] == 'release':
        stopped = next((s for s in after if s['speed'] <= STOP_SPEED), None)
        if stopped is None:
            raise RuntimeError('Did not reach stop threshold')
        row.update(stop_time_from_command_s=stopped['t'] - switch,
                   stop_distance_from_last_drive_sample_cm=stopped['location'][0] - reference['location'][0],
                   last_drive_sample_t=reference['t'], stop_sample_t=stopped['t'])
    elif case['name'] == 'reverse':
        zero = next((s for s in after if s['velocity'][0] <= 0), None)
        reverse95 = next((s for s in after if s['velocity'][0] <= -475), None)
        row.update(time_to_zero_x_s=zero['t'] - switch if zero else None,
                   time_to_reverse95_s=reverse95['t'] - switch if reverse95 else None,
                   forward_overshoot_cm=max(s['location'][0] for s in after) - reference['location'][0])
        if reverse95 is None:
            raise RuntimeError('Reversal failed to settle')
    elif case['name'] == 'turn90':
        settled = next((s for s in after if s['velocity'][1] >= 475 and abs(s['velocity'][0]) <= 25), None)
        row.update(time_to_new_direction95_s=settled['t'] - switch if settled else None,
                   minimum_turn_speed=min(s['speed'] for s in after if s['t'] <= switch + case['after']),
                   forward_carry_cm=after[-1]['location'][0] - reference['location'][0])
        if settled is None:
            raise RuntimeError('90-degree turn failed to settle')
    else:
        airborne = [s for s in after if s['falling']]
        landing = next((s for s in after if airborne and s['t'] > airborne[-1]['t'] and not s['falling']), None)
        if not airborne or landing is None:
            raise RuntimeError('Jump/landing not observed')
        row.update(takeoff_horizontal_speed=airborne[0]['speed'],
                   air_sample_duration_s=airborne[-1]['t'] - airborne[0]['t'],
                   peak_height_cm=max(s['location'][2] for s in airborne) - reference['location'][2],
                   horizontal_air_distance_cm=landing['location'][0] - airborne[0]['location'][0],
                   landing_horizontal_speed=landing['speed'])
    report['jobs'].append(row)
    persist()


def inject(key):
    # Используем реальные модификаторы сохранённой раскладки и существующие обработчики Character.
    mapping = keys[key]
    subsystem.inject_input_vector_for_action(mapping.get_editor_property('action'), unreal.Vector(1, 0, 0),
        mapping.get_editor_property('modifiers'), mapping.get_editor_property('triggers'))


def measure_tick(delta):
    if editor.get_game_world() != world:
        raise RuntimeError('PIE ended during measurement')
    now = unreal.GameplayStatics.get_time_seconds(world)
    if state['index'] < 0:
        begin(now)
        return
    profile, case = jobs[state['index']]
    t = now - state['start_t']
    velocity = pawn.get_velocity()
    state['samples'].append({'t': t, 'dt_slate': float(delta), 'velocity': xyz(velocity),
                             'location': xyz(pawn.get_actor_location()), 'speed': math.hypot(velocity.x, velocity.y),
                             'input': xyz(pawn.get_last_movement_input_vector()), 'falling': movement.is_falling()})
    switch = WARMUP + case['drive']
    if WARMUP <= t < switch:
        inject('W')
    elif switch <= t < switch + case['after']:
        if case['name'] == 'reverse':
            inject('S')
        elif case['name'] == 'turn90':
            inject('D')
        elif case['name'] == 'air_release':
            if t < switch + 0.06:
                inject('SpaceBar')
            if t < switch + 0.12:
                inject('W')
    if t >= switch + case['after'] + 0.2:
        summarize(profile, case)
        begin(now)


def tick(delta):
    # При ошибке снимаем callback и восстанавливаем параметры до следующего запуска.
    try:
        measure_tick(delta)
    except Exception as error:
        report['errors'].append(str(error))
        finish()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
builtins._sands_stage3_inertia = state
unreal.log('SANDS_STAGE3_INERTIA_STARTED ' + str(OUTPUT))
