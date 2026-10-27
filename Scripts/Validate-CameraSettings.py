"""Проверяет ссылку на DA_CameraManager и фактические настройки компонентов в PIE.
Запуск: py ".../Validate-CameraSettings.py" ".../report.json"
Не сохраняет ассеты и не подменяет проверку изображения/реального ввода.
"""
import json
import math
import sys
from pathlib import Path
import unreal

ASSET='/Game/Data/Camera/DA_CameraManager'
CHARACTER='/Game/Blueprints/Characters/BP_PlayerCharacter'
ARM_FIELDS=('target_arm_length','target_offset','socket_offset','do_collision_test','probe_size',
            'enable_camera_lag','camera_lag_speed','camera_lag_max_distance',
            'enable_camera_rotation_lag','camera_rotation_lag_speed')


# Векторы и скаляры сравниваются по значениям, без адресов временных Python-обёрток.
def value(obj,name):
    result=obj.get_editor_property(name)
    return [result.x,result.y,result.z] if name in ('target_offset','socket_offset') else result


# Статическая проверка читает только назначение; компонентные mirrors CDO не являются вторым источником настроек.
def inspect():
    asset=unreal.load_asset(ASSET)
    if not asset or asset.get_class().get_path_name()!='/Script/SandsOfSorrow.SandsOfSorrowCameraSettings':
        raise RuntimeError('Missing or unexpected camera Data Asset class')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        character=pc.get_controlled_pawn() if pc else None
    else:
        character=unreal.get_default_object(unreal.load_asset(CHARACTER).generated_class())
    if not character:
        raise RuntimeError('Missing player character')
    source=character.get_camera_settings()
    report={'project':unreal.Paths.get_project_file_path(),'pie':bool(world),'asset':asset.get_path_name(),
            'source':source.get_path_name() if source else None,'errors':[],
            'values':{name:value(asset,name) for name in ARM_FIELDS+('field_of_view',)}}
    if character.get_class().get_path_name()!=CHARACTER+'.BP_PlayerCharacter_C':
        report['errors'].append('Unexpected player character class')
    if source!=asset:
        report['errors'].append('Player character does not reference DA_CameraManager')
    if world:
        arm=character.get_editor_property('camera_boom')
        camera=character.get_editor_property('follow_camera')
        actual={name:value(arm,name) for name in ARM_FIELDS}
        actual['field_of_view']=value(camera,'field_of_view')
        report['runtime_values']=actual
        for name,expected in report['values'].items():
            pairs=zip(actual[name],expected) if isinstance(expected,list) else [(actual[name],expected)]
            if not all(math.isclose(a,b,rel_tol=1e-6,abs_tol=1e-6) for a,b in pairs):
                report['errors'].append('Runtime camera value differs from DA: '+name)
        manager=unreal.GameplayStatics.get_player_camera_manager(world,0)
        report['pitch_limits']=[manager.get_editor_property('view_pitch_min'),manager.get_editor_property('view_pitch_max')]
        report['look_sensitivity']=str(character.get_editor_property('look_sensitivity'))
        report['values_match']=not report['errors']
    return report


# Исключение также оставляет JSON-свидетельство для диагностики неудачного запуска.
output=Path(sys.argv[1])
try:
    report=inspect()
except Exception as error:
    report={'errors':[str(error)]}
output.parent.mkdir(parents=True,exist_ok=True)
output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if report['errors']:
    raise RuntimeError('Camera settings validation failed: '+'; '.join(report['errors']))
unreal.log('SANDS_CAMERA_SETTINGS_VALIDATION_OK '+str(output))
