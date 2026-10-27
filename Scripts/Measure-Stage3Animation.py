"""Измеряет ориентацию и опорные фазы стоп в обычном PIE; сохраняет только JSON/кадры.
Временные варианты ориентации/темпа восстанавливаются при завершении или ошибке.
"""
import builtins
import json
import math
import statistics
import sys
import traceback
from pathlib import Path
import unreal

output=Path(sys.argv[1])
profiles=json.loads(sys.argv[2]) if len(sys.argv)>2 else [{'name':'saved','strafe':None,'rate':None}]
old=getattr(builtins,'_sands_animation_probe',None)
if old and not old['done']: raise RuntimeError('Animation probe already active')
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=editor.get_game_world()
if not world or 'L_LightPrototype' not in world.get_path_name(): raise RuntimeError('Start ordinary PIE on L_LightPrototype')
pc=unreal.GameplayStatics.get_player_controller(world,0)
pawn=pc.get_controlled_pawn()
movement=pawn.get_editor_property('character_movement')
mesh=pawn.get_editor_property('mesh')
anim=mesh.get_anim_instance()
library=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
subsystem=library.call_method('GetLocalPlayerSubSystemFromPlayerController',(pc,unreal.EnhancedInputLocalPlayerSubsystem.static_class()))
action=pawn.get_editor_property('move_action')
context=pc.get_editor_property('player_mapping_context')
mappings=context.get_editor_property('default_key_mappings').get_editor_property('mappings')
keys={m.get_editor_property('key').export_text():m for m in mappings if m.get_editor_property('action')==action}
original={n:movement.get_editor_property(n) for n in ('orient_rotation_to_movement','use_controller_desired_rotation')}
original_rate=mesh.get_editor_property('global_anim_rate_scale')
original_location=pawn.get_actor_location(); original_rotation=pawn.get_actor_rotation(); original_control=pc.get_control_rotation()
def xyz(v): return [v.x,v.y,v.z]
def runtime_set(obj,name,v):
    # Editor notifications пересоздают Pawn и заново применяют DA; временный PIE-прогон их не вызывает.
    obj.set_editor_property(name,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def value(n):
    try: return mesh.get_anim_instance().get_editor_property(n)
    except Exception: return None
report={'project':unreal.Paths.get_project_file_path(),'physical_input':False,'method':'Saved Enhanced Input mappings; ball bone world-space stance proxy, not sole contact/IK proof',
 'mesh_scale':xyz(mesh.get_editor_property('relative_scale3d')),'anim_class':anim.get_class().get_path_name(),'original_orientation':original,
 'original_global_anim_rate':original_rate,'cases':[],'errors':[]}
cases=[dict(profile=p,name=name,keys=ks) for p in profiles for name,ks in [('W',['W']),('S',['S']),('A',['A']),('D',['D']),('WA',['W','A']),('WD',['W','D']),('SA',['S','A']),('SD',['S','D'])]]
state={'handle':None,'done':False,'index':-1,'start':0,'samples':[],'last':None}
builtins._sands_animation_probe=state

def write():
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,indent=2),encoding='utf-8')

def finish():
    # Callback снимается до восстановления; тест никогда не сохраняет runtime-настройки в ассеты.
    if state['done']: return
    state['done']=True
    unreal.unregister_slate_post_tick_callback(state['handle'])
    if editor.get_game_world()==world:
        movement.stop_movement_immediately()
        for n,v in original.items(): runtime_set(movement,n,v)
        runtime_set(mesh,'global_anim_rate_scale',original_rate)
        pawn.set_actor_location(original_location,False,True)
        pawn.set_actor_rotation(original_rotation,True)
        pc.set_control_rotation(original_control)
    write()

def begin(now):
    state['index']+=1
    if state['index']>=len(cases): finish(); return
    case=cases[state['index']]; profile=case['profile']
    movement.stop_movement_immediately(); pawn.consume_movement_input_vector()
    dx=(1 if 'W' in case['keys'] else 0)-(1 if 'S' in case['keys'] else 0)
    dy=(1 if 'D' in case['keys'] else 0)-(1 if 'A' in case['keys'] else 0)
    norm=math.hypot(dx,dy)
    pawn.set_actor_location(unreal.Vector(-1100-dx/norm*300,-700-dy/norm*300,77.15),False,True)
    pawn.set_actor_rotation(unreal.Rotator(),True)
    pc.set_control_rotation(unreal.Rotator(pitch=-15 if profile.get('capture') else 0,yaw=0,roll=0))
    for n,v in original.items(): runtime_set(movement,n,v)
    if profile.get('strafe') is not None:
        runtime_set(movement,'orient_rotation_to_movement',not profile['strafe'])
        runtime_set(movement,'use_controller_desired_rotation',profile['strafe'])
        assert movement.get_editor_property('orient_rotation_to_movement')==(not profile['strafe'])
        assert movement.get_editor_property('use_controller_desired_rotation')==profile['strafe']
    runtime_set(mesh,'global_anim_rate_scale',profile.get('rate') or original_rate)
    state.update(start=now,samples=[],last=None,captured=False)

def complete():
    case=dict(cases[state['index']]); samples=state['samples']
    steady=[s for s in samples if 0.9<s['t']<1.7]
    if len(steady)<20: raise RuntimeError('Insufficient animation samples; disable background throttling for measurement')
    metrics={}
    for foot in ('ball_l','ball_r'):
        low=min(s[foot][2] for s in steady)
        slips=[]
        for a,b in zip(steady,steady[1:]):
            dt=b['t']-a['t']
            if dt>0 and a[foot][2]<=low+2 and b[foot][2]<=low+2:
                slips.append(math.hypot(b[foot][0]-a[foot][0],b[foot][1]-a[foot][1])/dt)
        metrics[foot]={'min_z':low,'stance_samples':len(slips),'median_world_slip_cm_s':statistics.median(slips) if slips else None}
    if any(m['stance_samples']<3 for m in metrics.values()): raise RuntimeError('Too few stance samples for '+case['name'])
    case.update(speed_mean=statistics.mean(s['speed'] for s in steady),yaw_final=steady[-1]['yaw'],direction_final=steady[-1]['direction'],
                ground_speed_final=steady[-1]['ground_speed'],feet=metrics,samples=samples)
    report['cases'].append(case); write()

def tick(dt):
    try:
        if editor.get_game_world()!=world: raise RuntimeError('PIE ended during animation probe')
        now=unreal.GameplayStatics.get_time_seconds(world)
        if state['index']<0: begin(now); return
        t=now-state['start']; case=cases[state['index']]
        if t>2.1: complete(); begin(now); return
        if case['profile'].get('capture') and t>=1.15 and not state['captured']:
            state['captured']=True
            unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(output.parent/('strafe-'+case['name']+'.png')))
        velocity=pawn.get_velocity()
        row={'t':t,'speed':math.hypot(velocity.x,velocity.y),'yaw':pawn.get_actor_rotation().yaw,
             'position':xyz(pawn.get_actor_location()),'direction':value('Direction'),'ground_speed':value('GroundSpeed'),'falling':movement.is_falling()}
        for bone in ('ball_l','ball_r','foot_l','foot_r'): row[bone]=xyz(mesh.get_socket_location(bone))
        state['samples'].append(row)
        if 0.3<t<1.8:
            for key in case['keys']:
                m=keys[key]
                subsystem.inject_input_vector_for_action(action,unreal.Vector(1,0,0),m.get_editor_property('modifiers'),m.get_editor_property('triggers'))
    except Exception:
        report['errors'].append(traceback.format_exc()); finish()

state['handle']=unreal.register_slate_post_tick_callback(tick)
