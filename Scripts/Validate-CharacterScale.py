"""Проверяет уменьшенный Pawn в обычном PIE программным вводом и физическими коллизиями.
Временные препятствия существуют только в PIE; карта и ассеты не сохраняются.
Запуск: py ".../Validate-CharacterScale.py" ".../physics.json"
"""
import builtins
import json
import math
import sys
import time
from pathlib import Path
import unreal


# Изолированный запуск владеет callback и временными Actor; завершение и ошибка освобождают их.
def run(output):
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world=editor.get_game_world()
    if not world or 'L_LightPrototype' not in world.get_path_name():
        raise RuntimeError('Start ordinary PIE on L_LightPrototype')
    previous=getattr(builtins,'_sands_scale_physics',None)
    if previous and not previous['done']:
        raise RuntimeError('A scale validation is already running')
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    pawn=pc.get_controlled_pawn()
    movement=pawn.get_editor_property('character_movement')
    capsule=pawn.get_editor_property('capsule_component')
    half=capsule.get_editor_property('capsule_half_height')
    radius=capsule.get_editor_property('capsule_radius')
    original=pawn.get_actor_transform()
    original_rotation=pc.get_control_rotation()
    actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
    door=next(a for a in actors if a.get_actor_label()=='PuzzleDoor_Step6')
    cube=unreal.load_asset('/Engine/BasicShapes/Cube')
    fixtures=[]
    report={'mode':'ordinary PIE, programmatic movement; not physical keyboard input','cases':[], 'errors':[]}
    state={'done':False,'handle':None,'index':-1,'start':0,'samples':[], 'jumped':False}
    clock=time.monotonic()
    cases=[
        {'name':'ground_run_stop','start':(-1300,-250),'direction':(1,0),'drive':1.4,'duration':2.1},
        {'name':'standing_jump','start':(-1300,-250),'direction':(0,0),'drive':0,'jump':0.2,'duration':1.9},
        {'name':'wall','start':(-1200,1000),'direction':(0,1),'drive':1.0,'duration':1.3},
        {'name':'corner_slide','start':(400,450),'direction':(1,0.4),'drive':1.0,'duration':1.3},
        {'name':'step_20','step':20}, {'name':'step_40','step':40}, {'name':'step_50_blocked','step':50},
        {'name':'stairs_20_40','stairs':True},
        {'name':'clearance_155','ceiling':155}, {'name':'clearance_145_blocked','ceiling':145},
        {'name':'width_80','gap':80}, {'name':'width_60_blocked','gap':60},
        {'name':'existing_platform_50','start':(-300,-550),'direction':(1,0),'drive':0.75,'jump':0.15,'duration':2.1,'platform':50},
        {'name':'existing_platform_100','start':(450,-850),'direction':(0,1),'drive':0.75,'jump':0.15,'duration':2.1,'platform':100},
        {'name':'closed_door','start':(150,0),'direction':(1,0),'drive':1.0,'duration':1.3},
        {'name':'open_door_traversal','start':(150,0),'direction':(1,0),'drive':1.0,'duration':2.8,'warmup':1.5},
    ]

    # Препятствия создаются в игровом World через deferred spawn, не в редакторской карте.
    def box(location,dimensions):
        transform=unreal.Transform()
        transform.set_editor_property('translation',unreal.Vector(*location))
        transform.set_editor_property('scale3d',unreal.Vector(*(v/100 for v in dimensions)))
        # BlueprintInternalUseOnly скрывает Python-обёртки; используем подтверждённые UFUNCTION через reflection.
        library=unreal.get_default_object(unreal.GameplayStatics.static_class())
        actor=library.call_method('BeginDeferredActorSpawnFromClass',
                    (world,unreal.StaticMeshActor.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN))
        fixtures.append(actor)
        component=actor.get_editor_property('static_mesh_component')
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(cube)
        component.set_collision_profile_name('BlockAll')
        library.call_method('FinishSpawningActor',(actor,transform))

    def clear_fixtures():
        for actor in fixtures:
            if unreal.SystemLibrary.is_valid(actor):
                actor.destroy_actor()
        fixtures.clear()

    def finish():
        if state['done']:
            return
        state['done']=True
        unreal.unregister_slate_post_tick_callback(state['handle'])
        clear_fixtures()
        if unreal.SystemLibrary.is_valid(pawn):
            movement.stop_movement_immediately()
            pawn.stop_jumping()
            pawn.set_actor_transform(original,False,True)
            pc.set_control_rotation(original_rotation)
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps(report,indent=2),encoding='utf-8')
        unreal.log('SANDS_SCALE_PHYSICS_FINISHED '+str(output))

    def next_case():
        clear_fixtures()
        state['index']+=1
        if state['index']>=len(cases):
            finish()
            return
        case=cases[state['index']]
        movement.stop_movement_immediately()
        pawn.consume_movement_input_vector()
        pawn.stop_jumping()
        movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
        start=case.get('start',(-1200,-900))
        pawn.set_actor_location_and_rotation(unreal.Vector(start[0],start[1],half+2.15),unreal.Rotator(),False,True)
        pc.set_control_rotation(unreal.Rotator())
        if 'step' in case:
            h=case['step']; box((-900,-900,h/2),(100,200,h))
        if case.get('stairs'):
            box((-1000,-900,10),(100,200,20)); box((-900,-900,20),(100,200,40))
        if 'ceiling' in case:
            box((-900,-900,case['ceiling']+5),(100,200,10))
        if 'gap' in case:
            for sign in (-1,1):
                box((-900,-900+sign*(case['gap']/2+10),100),(200,20,200))
        if case['name']=='open_door_traversal':
            door.set_open(True)
        state.update(start=unreal.GameplayStatics.get_time_seconds(world),samples=[],jumped=False)

    # Критерии проверяют исход движения, а не факт вызова AddMovementInput.
    def evaluate(case,samples):
        end=samples[-1]['position']; start=samples[0]['position']
        peak=max(s['position'][2] for s in samples)
        speed=max(s['speed'] for s in samples)
        name=case['name']
        if name=='ground_run_stop': passed=abs(speed-500)<1 and samples[-1]['speed']<1 and end[0]>-800
        elif name=='standing_jump': passed=110<peak-start[2]<135 and abs(end[2]-(half+2.15))<1 and abs(end[0]-start[0])<1
        elif name=='wall': passed=abs(end[1]-(1200-radius))<1
        elif name=='corner_slide': passed=end[0]<=600-radius+1 and end[1]>550
        elif name=='closed_door': passed=abs(end[0]-(288-radius))<1
        elif name=='open_door_traversal': passed=end[0]>400
        elif 'platform' in case: passed=abs(end[2]-(half+2.15+case['platform']))<1 and not samples[-1]['falling']
        elif 'blocked' in name: passed=end[0]<-940
        else: passed=end[0]>-800 and (peak>=half+2.15+case.get('step',40 if case.get('stairs') else 0)-1)
        row={'name':name,'passed':passed,'end':end,'peak_z':peak,'max_speed':speed,'samples':samples}
        report['cases'].append(row)
        if not passed:
            report['errors'].append(name)

    def tick(dt):
        try:
            if not unreal.SystemLibrary.is_valid(pawn) or time.monotonic()-clock>180:
                raise RuntimeError('PIE ended or validation timeout')
            if state['index']<0:
                next_case()
                return
            case=cases[state['index']]
            elapsed=unreal.GameplayStatics.get_time_seconds(world)-state['start']
            warmup=case.get('warmup',0.2)
            drive=case.get('drive',1.5)
            if warmup<=elapsed<warmup+drive:
                direction=case.get('direction',(1,0))
                length=math.hypot(*direction)
                if length:
                    pawn.add_movement_input(unreal.Vector(direction[0]/length,direction[1]/length,0),1)
            if 'jump' in case and not state['jumped'] and elapsed>=warmup+case['jump']:
                pawn.jump(); state['jumped']=True
            pos=pawn.get_actor_location(); velocity=pawn.get_velocity()
            state['samples'].append({'t':elapsed,'position':[pos.x,pos.y,pos.z],
                    'speed':math.hypot(velocity.x,velocity.y),'falling':movement.is_falling()})
            if elapsed>=case.get('duration',2.3):
                evaluate(case,state['samples']); next_case()
        except Exception as error:
            report['errors'].append(str(error)); finish()
    state['handle']=unreal.register_slate_post_tick_callback(tick)
    builtins._sands_scale_physics=state


run(Path(sys.argv[1]))
