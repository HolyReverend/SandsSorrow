"""Проверяет crouch в обычном PIE программным вводом; не заменяет реальную клавиатуру/приёмку.
Запуск: py ".../Scripts/Validate-Crouch.py" ".../report.json"
Временная геометрия, FPS и управление очищаются также при ошибке; ассеты не сохраняются.
"""
import builtins
import json
import math
import statistics
import sys
import time
import traceback
from pathlib import Path
import unreal


def run(output):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if not world or 'L_LightPrototype' not in world.get_path_name():
        raise RuntimeError('Start ordinary PIE on L_LightPrototype')
    previous = getattr(builtins, '_sands_crouch_probe', None)
    if previous and not previous['done']:
        raise RuntimeError('Crouch probe is already running')
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = pc.get_controlled_pawn()
    movement = pawn.character_movement
    capsule = pawn.capsule_component
    original = pawn.get_actor_transform()
    control = pc.get_control_rotation()
    controller_tick = pc.is_actor_tick_enabled()
    held = pawn.is_crouch_held()
    max_fps = unreal.SystemLibrary.get_console_variable_float_value('t.MaxFPS')
    perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
    throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
    standing_half = unreal.get_default_object(pawn.get_class()).capsule_component.get_editor_property('capsule_half_height')
    base_speed = movement.get_editor_property('max_walk_speed')
    crouch_half = movement.get_editor_property('crouched_half_height')
    report = {'project': unreal.Paths.get_project_file_path(), 'physical_input': False,
              'method': 'ordinary PIE; native hold requests and movement input; controller tick temporarily disabled to isolate probe from UI focus',
              'tolerances': {'speed_cm_s': 2, 'height_cm': 0.1, 'stopped_cm_s': 1},
              'base_speed': base_speed, 'standing_half_height': standing_half, 'crouch_half_height': crouch_half,
              'cases': [], 'errors': []}
    cases = [(fps, 'standing', 0) for fps in (30, 60, 120)]
    cases += [(fps, 'crouch', angle) for fps in (30, 60, 120) for angle in range(0, 360, 45)]
    cases += [(fps, name, 0) for fps in (30, 60, 120) for name in ('stop_standing', 'stop_crouch', 'tunnel_jump', 'animation_interruptions')]
    fixtures = []
    clock = time.monotonic()
    state = {'done': False, 'handle': None, 'index': -1, 'phase': 'next', 'samples': [], 'flags': {}}
    builtins._sands_crouch_probe = state
    pc.set_actor_tick_enabled(False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground', False)

    def valid():
        return editor.get_game_world() == world and unreal.SystemLibrary.is_valid(pawn) and pc.get_controlled_pawn() == pawn

    def clear_fixtures():
        for actor in fixtures:
            if unreal.SystemLibrary.is_valid(actor):
                actor.destroy_actor()
        fixtures.clear()

    # Это только временная коллизия игрового World; сохранённая карта не меняется.
    def box(location, scale):
        transform = unreal.Transform()
        transform.set_editor_property('translation', unreal.Vector(*location))
        transform.set_editor_property('scale3d', unreal.Vector(*scale))
        library = unreal.get_default_object(unreal.GameplayStatics.static_class())
        actor = library.call_method('BeginDeferredActorSpawnFromClass',
            (world, unreal.StaticMeshActor.static_class(), transform, unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN))
        fixtures.append(actor)
        component = actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
        component.set_collision_profile_name('BlockAll')
        library.call_method('FinishSpawningActor', (actor, transform))

    def canopy():
        box((-1100, -700, 120), (4, 3, 0.1))
        for side in (-1, 1):
            box((-1100, -700 + side * 46, 75), (4, 0.2, 1.5))

    def write():
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2), encoding='utf8')

    def finish():
        if state['done']:
            return
        state['done'] = True
        try:
            if state['handle'] is not None:
                unreal.unregister_slate_post_tick_callback(state['handle'])
            clear_fixtures()
            if valid():
                pawn.stop_jumping()
                pawn.set_crouch_held(held)
                movement.stop_movement_immediately()
                pawn.consume_movement_input_vector()
                pawn.set_actor_transform(original, False, True)
                pc.set_control_rotation(control)
        except Exception:
            report['errors'].append('Pawn cleanup: ' + traceback.format_exc())
        finally:
            try:
                if unreal.SystemLibrary.is_valid(pc):
                    pc.set_actor_tick_enabled(controller_tick)
            except Exception:
                report['errors'].append('Controller cleanup: ' + traceback.format_exc())
            try:
                unreal.SystemLibrary.execute_console_command(editor.get_game_world() or editor.get_editor_world(), 't.MaxFPS ' + str(max_fps))
                perf.set_editor_property('bThrottleCPUWhenNotForeground', throttle)
            except Exception:
                report['errors'].append('Global cleanup: ' + traceback.format_exc())
            # Сравнение остановки при одинаковой начальной скорости отделяет профиль инерции от лимита скорости.
            report['comparisons'] = []
            for fps in (30,60,120):
                standing = next((c for c in report['cases'] if c['name']=='stop_standing' and c['fps_limit']==fps),None)
                crouching = next((c for c in report['cases'] if c['name']=='stop_crouch' and c['fps_limit']==fps),None)
                if standing and crouching:
                    passed = crouching['stopping_distance_cm'] < standing['stopping_distance_cm'] * 0.8 and crouching['stopping_time_s'] < standing['stopping_time_s']
                    report['comparisons'].append({'fps_limit':fps,'less_inertia':passed,'standing_time':standing['stopping_time_s'],'crouch_time':crouching['stopping_time_s'],'standing_distance':standing['stopping_distance_cm'],'crouch_distance':crouching['stopping_distance_cm']})
                    if not passed:report['errors'].append('same-speed inertia@'+str(fps))
            report['finished'] = True
            report['restored'] = {'fps': unreal.SystemLibrary.get_console_variable_float_value('t.MaxFPS'),
                                  'controller_tick': pc.is_actor_tick_enabled() if unreal.SystemLibrary.is_valid(pc) else None}
            write()
            unreal.log('SANDS_CROUCH_FINISHED ' + str(output))

    def next_case(now):
        clear_fixtures()
        state['index'] += 1
        if state['index'] >= len(cases):
            finish()
            return
        fps, name, angle = cases[state['index']]
        unreal.SystemLibrary.execute_console_command(world, 't.MaxFPS ' + str(fps))
        pawn.stop_jumping()
        pawn.set_crouch_held(False)
        movement.stop_movement_immediately()
        pawn.consume_movement_input_vector()
        movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
        pawn.set_actor_location_and_rotation(unreal.Vector(-1100, -700, standing_half + 2.15), unreal.Rotator(), False, True)
        pc.set_control_rotation(unreal.Rotator(pitch=-15, yaw=0, roll=0))
        state.update(phase='settle', start=now, fps=fps, name=name, angle=angle, samples=[], stance_samples=[], flags={})

    def row(now):
        pos = pawn.get_actor_location()
        vel = pawn.get_velocity()
        camera = pawn.get_editor_property('follow_camera').get_world_location()
        anim = pawn.mesh.get_anim_instance()
        head = pawn.mesh.get_socket_location('head')
        return {'animation_crouch': anim.get_editor_property('IsCrouched'),
                'blend_xy': [anim.get_editor_property('CrouchX'), anim.get_editor_property('CrouchY')],
                'head_z': head.z, 't': now - state['start'], 'position': [pos.x, pos.y, pos.z],
                'speed': math.hypot(vel.x, vel.y), 'falling': movement.is_falling(),
                'crouched': pawn.get_editor_property('is_crouched'), 'held': pawn.is_crouch_held(),
                'half_height': capsule.get_editor_property('capsule_half_height'),
                'camera': [camera.x, camera.y, camera.z] if camera else None}

    def complete():
        rows = state['samples']
        errors = []
        name = state['name']
        if name in ('standing', 'crouch'):
            drive = [r for r in rows if 0.75 < r['t'] < 1.1]
            expected = base_speed * (0.5 if name == 'crouch' else 1.0)
            if not drive or abs(statistics.median(r['speed'] for r in drive) - expected) > 2:
                errors.append('steady speed')
            if name == 'crouch' and not all(r['crouched'] and abs(r['half_height']-crouch_half)<0.1 for r in drive):
                errors.append('physical crouch')
            if rows[-1]['speed'] > 1:
                errors.append('stopping')
        elif name == 'tunnel_jump':
            blocked = [r for r in rows if 0.8 < r['t'] < 1.0]
            if not blocked or not all(r['crouched'] and not r['held'] and not r['falling'] for r in blocked):
                errors.append('blocked stand/refused jump')
            after = [r for r in rows if 2.75 < r['t'] < 2.95]
            if not after or any(r['crouched'] or r['falling'] for r in after):
                errors.append('automatic safe stand/no buffered jump')
            if not any(r['falling'] for r in rows if 3.05 < r['t'] < 3.6):
                errors.append('fresh jump after exit')
        elif name.startswith('stop_'):
            if rows[-1]['speed'] > 1:
                errors.append('seeded stopping')
        elif name == 'animation_interruptions':
            for low, high, label in [(0.5, 0.75, 'jump during enter'), (1.35, 1.55, 'jump during exit')]:
                if not any(r['falling'] for r in rows if low < r['t'] < high):
                    errors.append(label)
            settled = [r for r in rows if 2.9 < r['t'] < 3.2]
            if not settled or not all(r['crouched'] and r['animation_crouch'] for r in settled):
                errors.append('crouched animation reinitialization')
        if name == 'crouch':
            driven = [r for r in rows if 0.75 < r['t'] < 1.1]
            if not all(r['animation_crouch'] for r in driven):
                errors.append('animation stance cache')
            angle = math.radians(state['angle'])
            denominator = max(abs(math.sin(angle)), abs(math.cos(angle)))
            expected_xy = [51.5 + 48.5 * math.sin(angle) / denominator,
                           51.5 + 48.5 * math.cos(angle) / denominator]
            if any(abs(statistics.median(r['blend_xy'][axis] for r in driven) - expected_xy[axis]) > 4 for axis in (0, 1)):
                errors.append('directional BlendSpace input')
            changes = [abs(b['camera'][2] - a['camera'][2]) for a,b in zip(state['stance_samples'],state['stance_samples'][1:])]
            if changes and max(changes) > 8:
                errors.append('camera height discontinuity')
        dts = [b['t']-a['t'] for a,b in zip(rows,rows[1:]) if b['t'] > a['t']]
        result = {'name': name, 'angle': state['angle'], 'fps_limit': state['fps'],
                  'actual_fps_median': 1/statistics.median(dts) if dts else None,
                  'passed': not errors, 'errors': errors, 'samples': rows, 'stance_samples': state['stance_samples']}
        if name.startswith('stop_'):
            stopped = next((r for r in rows if r['speed'] <= 1), rows[-1])
            result['stopping_time_s'] = stopped['t']
            result['stopping_distance_cm'] = math.hypot(stopped['position'][0]-rows[0]['position'][0],stopped['position'][1]-rows[0]['position'][1])
        report['cases'].append(result)
        report['errors'].extend(name + '@' + str(state['fps']) + ': ' + e for e in errors)
        write()
        state['phase'] = 'next'

    def tick(dt):
        try:
            if not valid() or time.monotonic() - clock > 240:
                raise RuntimeError('PIE/Pawn changed or probe timed out')
            now = unreal.GameplayStatics.get_time_seconds(world)
            if state['phase'] == 'next':
                next_case(now)
                return
            if state['phase'] == 'settle':
                if now - state['start'] < 0.25:
                    return
                pawn.set_crouch_held(state['name'] in ('crouch', 'stop_crouch', 'tunnel_jump'))
                state.update(phase='stance_settle', start=now)
            if state['phase'] == 'stance_settle':
                state['stance_samples'].append(row(now))
                if now - state['start'] < 0.3:
                    return
                if state['name'].startswith('stop_'):
                    movement.set_editor_property('velocity',unreal.Vector(base_speed*0.5,0,0),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
                state.update(phase='measure', start=now)
            elapsed = now - state['start']
            if state['name'] in ('standing', 'crouch'):
                if elapsed < 1.1:
                    angle = math.radians(state['angle'])
                    pawn.add_movement_input(unreal.Vector(math.cos(angle), math.sin(angle), 0), 1.0)
                duration = 1.8
            elif state['name'].startswith('stop_'):
                duration = 0.6
            elif state['name'] == 'animation_interruptions':
                flags = state['flags']
                actions = [(0.0,'on',True),(0.12,'off',False),(0.22,'on2',True),(1.1,'off2',False),(2.0,'on3',True)]
                for at, key, active in actions:
                    if elapsed >= at and key not in flags:
                        pawn.set_crouch_held(active)
                        flags[key] = True
                for at,key in [(0.4,'enter_jump'),(1.25,'exit_jump')]:
                    if elapsed >= at and key not in flags:
                        pawn.jump()
                        flags[key] = True
                if elapsed >= 2.7 and 'reinit' not in flags:
                    pawn.mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_BLUEPRINT,True)
                    flags['reinit'] = True
                duration = 3.3
            else:
                flags = state['flags']
                if elapsed > 0.25 and 'canopy' not in flags:
                    pawn.set_actor_location(unreal.Vector(-1200, -700, crouch_half + 2.15), False, True)
                    canopy()
                    flags['canopy'] = True
                if elapsed > 0.5:
                    pawn.set_crouch_held(False)
                if elapsed > 0.65 and 'refused' not in flags:
                    pawn.jump()
                    flags['refused'] = True
                if 1.05 < elapsed < 2.65:
                    pawn.add_movement_input(unreal.Vector(1, 0, 0), 1.0)
                if elapsed > 3.0 and 'fresh' not in flags:
                    pawn.stop_jumping()
                    pawn.jump()
                    flags['fresh'] = True
                duration = 3.8
            state['samples'].append(row(now))
            if elapsed > duration:
                complete()
        except Exception:
            report['errors'].append(traceback.format_exc())
            finish()

    state['handle'] = unreal.register_slate_post_tick_callback(tick)


run(Path(sys.argv[1]))
