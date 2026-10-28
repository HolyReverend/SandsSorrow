"""Измеряет прыжок в обычном PIE; не сохраняет ассеты и восстанавливает временные параметры.
Запуск: py ".../Scripts/Validate-Jump.py" ".../report.json" [saved|baseline]
Это программный ввод/телеметрия, а не физическая клавиатура или авторская приёмка.
"""
import builtins
import json
import math
import statistics
import sys
import traceback
from pathlib import Path
import unreal

output = Path(sys.argv[1])
profile = sys.argv[2] if len(sys.argv) > 2 else 'saved'
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
if not world or 'L_LightPrototype' not in world.get_path_name():
    raise RuntimeError('Start ordinary PIE on L_LightPrototype')
old = getattr(builtins, '_sands_jump_probe', None)
if old and not old['done']:
    raise RuntimeError('Jump probe already running')
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = pc.get_controlled_pawn()
movement = pawn.get_editor_property('character_movement')
fields = ('jump_z_velocity', 'gravity_scale', 'air_control', 'air_control_boost_multiplier',
          'air_control_boost_velocity_threshold', 'falling_lateral_friction', 'braking_deceleration_falling')
original = {n: movement.get_editor_property(n) for n in fields}
location = pawn.get_actor_location()
rotation = pawn.get_actor_rotation()
control = pc.get_control_rotation()
perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
max_fps = unreal.SystemLibrary.get_console_variable_float_value('t.MaxFPS')

def set_runtime(obj, name, value):
    obj.set_editor_property(name, value, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)

def xyz(v):
    return [v.x, v.y, v.z]

if profile not in ('saved', 'baseline'):
    raise RuntimeError('Unknown profile')
cases = [(fps, name, speed, direction) for fps in ([120] if profile == 'baseline' else [30, 60, 120])
         for name, speed, direction in [('standing', (0, 0), (0, 0)),
                                       ('release', (500, 0), (0, 0)),
                                       ('forward', (500, 0), (1, 0)),
                                       ('opposite', (500, 0), (-1, 0)),
                                       ('lateral', (500, 0), (0, 1)),
                                       ('diagonal', (353.55339, 353.55339), (0, 0)),
                                       ('near_zero', (0, 0), (0, 1)),
                                       ('camera_turn', (500, 0), (0, 0))]]
report = {'project': unreal.Paths.get_project_file_path(), 'profile': profile,
          'method': 'ordinary PIE, Jump request and AddMovementInput; initial velocity seeded once',
          'settings': {n: movement.get_editor_property(n) for n in fields},
          'physical_input': False, 'tolerances': {'apex_cm': 8, 'airtime_s': 0.08, 'range_cm': 25},
          'cases': [], 'errors': []}
state = {'done': False, 'handle': None, 'index': -1, 'phase': 'next', 'samples': [], 'initialized': False}
builtins._sands_jump_probe = state

def write():
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')

def finish():
    if state['done']:
        return
    state['done'] = True
    try:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        if (editor.get_game_world() == world and unreal.SystemLibrary.is_valid(pawn)
                and unreal.SystemLibrary.is_valid(movement) and unreal.SystemLibrary.is_valid(pc)
                and pc.get_controlled_pawn() == pawn):
            pawn.stop_jumping()
            movement.stop_movement_immediately()
            pawn.consume_movement_input_vector()
            for n, v in original.items():
                set_runtime(movement, n, v)
            pawn.set_actor_location(location, False, True)
            pawn.set_actor_rotation(rotation, True)
            pc.set_control_rotation(control)
    except Exception:
        report['errors'].append('Pawn cleanup: ' + traceback.format_exc())
    finally:
        # Глобальные параметры восстанавливаются независимо от жизни Pawn и ошибок его очистки.
        try:
            unreal.SystemLibrary.execute_console_command(editor.get_game_world() or editor.get_editor_world(), 't.MaxFPS ' + str(max_fps))
        except Exception:
            report['errors'].append('FPS cleanup: ' + traceback.format_exc())
        try:
            perf.set_editor_property('bThrottleCPUWhenNotForeground', throttle)
        except Exception:
            report['errors'].append('Throttle cleanup: ' + traceback.format_exc())
        report['restored'] = {'fps': unreal.SystemLibrary.get_console_variable_float_value('t.MaxFPS'),
                              'throttle': perf.get_editor_property('bThrottleCPUWhenNotForeground') == throttle}
        write()

def next_case(now):
    if not state['initialized']:
        state['initialized'] = True
        if profile == 'baseline':
            values = dict(jump_z_velocity=500.0, gravity_scale=1.0, air_control=0.35,
                          air_control_boost_multiplier=2.0, air_control_boost_velocity_threshold=25.0,
                          falling_lateral_friction=0.0, braking_deceleration_falling=1500.0)
            for n, v in values.items():
                set_runtime(movement, n, v)
        perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
        report['settings'] = {n: movement.get_editor_property(n) for n in fields}
    state['index'] += 1
    if state['index'] >= len(cases):
        finish()
        return
    fps, name, speed, direction = cases[state['index']]
    unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS ' + str(fps))
    pawn.stop_jumping()
    movement.stop_movement_immediately()
    pawn.consume_movement_input_vector()
    pawn.set_actor_location(unreal.Vector(-1450, -700, 77.15), False, True)
    movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    pawn.set_actor_rotation(unreal.Rotator(), True)
    pc.set_control_rotation(unreal.Rotator(pitch=-15, yaw=0, roll=0))
    state.update(phase='settle', start=now, samples=[], airborne=False, fps=fps,
                 name=name, speed=speed, direction=direction)

def complete(now):
    rows = state['samples']
    airborne = [r for r in rows if r['falling']]
    if len(airborne) < 8:
        raise RuntimeError('Insufficient flight samples: ' + state['name'])
    dts = [b['t'] - a['t'] for a, b in zip(rows, rows[1:]) if b['t'] > a['t']]
    start = state['takeoff_position']
    apex = max(rows, key=lambda r: r['position'][2])
    last_air = airborne[-1]
    takeoff_speed = math.hypot(*state['speed'])
    height = apex['position'][2] - start[2]
    airtime = now - state['jump_time']
    displacement = math.hypot(rows[-1]['position'][0]-start[0], rows[-1]['position'][1]-start[1])
    gravity = abs(movement.get_gravity_z())
    expected_height = report['settings']['jump_z_velocity']**2/(2*gravity)
    expected_time = 2*report['settings']['jump_z_velocity']/gravity
    errors = []
    if abs(height-expected_height)>8: errors.append('apex outside tolerance')
    if abs(airtime-expected_time)>0.08: errors.append('airtime outside tolerance')
    if state['name'] in ('release', 'diagonal', 'camera_turn') and profile == 'saved':
        if abs(math.hypot(*last_air['velocity'][:2])-takeoff_speed)>2: errors.append('inertia changed without input')
    if state['name'] == 'standing' and displacement>0.1: errors.append('standing horizontal impulse')
    if profile == 'saved' and state['name'] in ('lateral', 'near_zero', 'opposite'):
        dv = math.hypot(last_air['velocity'][0]-state['speed'][0], last_air['velocity'][1]-state['speed'][1])
        response = dv/max(last_air['t'], 0.001)/movement.get_editor_property('max_acceleration')
        if not 0.075 < response < 0.125: errors.append('air correction response outside 7.5-12.5 percent')
    else:
        response = None
    report['cases'].append({'name': state['name'], 'fps_limit': state['fps'],
                            'actual_fps_median': 1/statistics.median(dts),
                            'initial_velocity': state['speed'], 'height_cm': height,
                            'time_to_apex_s': apex['t'], 'airtime_s': airtime,
                            'range_cm': displacement, 'last_air_velocity': last_air['velocity'],
                            'air_acceleration_fraction': response, 'errors': errors, 'samples': rows})
    report['errors'].extend(state['name'] + '@' + str(state['fps']) + ': ' + e for e in errors)
    write()
    state['phase'] = 'next'

def tick(dt):
    try:
        if editor.get_game_world() != world:
            raise RuntimeError('PIE changed during jump probe')
        now = unreal.GameplayStatics.get_time_seconds(world)
        if state['phase'] == 'next':
            next_case(now)
            return
        if state['phase'] == 'settle':
            if now-state['start'] < 0.3:
                return
            if movement.is_falling():
                raise RuntimeError('Fixture not grounded')
            set_runtime(movement, 'velocity', unreal.Vector(*state['speed'], 0))
            state['takeoff_position'] = xyz(pawn.get_actor_location())
            state['jump_time'] = now
            pawn.jump()
            state['phase'] = 'flight'
        else:
            pos = pawn.get_actor_location()
            vel = pawn.get_velocity()
            falling = movement.is_falling()
            elapsed = now-state['jump_time']
            state['samples'].append({'t': elapsed, 'position': xyz(pos), 'velocity': xyz(vel), 'falling': falling})
            state['airborne'] |= falling
            if state['airborne'] and not falling:
                complete(now)
                return
            if elapsed > 2:
                raise RuntimeError('No takeoff/landing: ' + state['name'])
        dx, dy = state['direction']
        if dx or dy:
            pawn.add_movement_input(unreal.Vector(dx, dy, 0), 1.0)
        if state['name'] == 'camera_turn' and now-state['jump_time'] > 0.12:
            pc.set_control_rotation(unreal.Rotator(pitch=-15, yaw=90, roll=0))
    except Exception:
        report['errors'].append(traceback.format_exc())
        finish()

state['handle'] = unreal.register_slate_post_tick_callback(tick)
