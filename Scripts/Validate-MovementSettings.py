"""Проверяет сохранённую ссылку на DA или настройки фактического Pawn в PIE.
Запуск: py ".../Scripts/Validate-MovementSettings.py" ".../report.json"
Не меняет и не сохраняет ассеты. Проверка значений не доказывает реальный ввод.
"""
import json
import math
import sys
from datetime import datetime, timezone
from pathlib import Path
import unreal

ASSET = '/Game/Data/Characters/DA_CharacterMovement'
CHARACTER = '/Game/Blueprints/Characters/BP_PlayerCharacter'
MOVEMENT_FIELDS = (
    'max_walk_speed', 'min_analog_walk_speed', 'max_acceleration',
    'braking_deceleration_walking', 'ground_friction', 'use_separate_braking_friction',
    'braking_friction', 'braking_friction_factor', 'orient_rotation_to_movement',
    'use_controller_desired_rotation', 'jump_z_velocity', 'gravity_scale', 'air_control',
    'air_control_boost_multiplier', 'air_control_boost_velocity_threshold',
    'falling_lateral_friction', 'braking_deceleration_falling',
)
CHARACTER_FIELDS = ('use_controller_rotation_yaw', 'jump_max_count', 'jump_max_hold_time')


# Статическая проверка читает ссылку CDO; в PIE сравнивает источник со всеми фактическими настройками.
def inspect():
    asset = unreal.load_asset(ASSET)
    if not asset or asset.get_class().get_path_name() != '/Script/SandsOfSorrow.SandsOfSorrowCharacterMovementSettings':
        raise RuntimeError('Missing or unexpected movement Data Asset class')
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if world:
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        character = controller.get_controlled_pawn() if controller else None
    else:
        character = unreal.get_default_object(unreal.load_asset(CHARACTER).generated_class())
    if not character:
        raise RuntimeError('Missing player character')
    expected_class = CHARACTER + '.BP_PlayerCharacter_C'
    report = {'timestamp_utc': datetime.now(timezone.utc).isoformat(),
              'project': unreal.Paths.get_project_file_path(), 'pie': bool(world),
              'asset': asset.get_path_name(), 'character_class': character.get_class().get_path_name(),
              'errors': []}
    if report['character_class'] != expected_class:
        report['errors'].append('Unexpected player character class')
    source = character.get_movement_settings()
    report['source'] = source.get_path_name() if source else None
    if source != asset:
        report['errors'].append('Player character does not reference DA_CharacterMovement')
    fields = MOVEMENT_FIELDS + CHARACTER_FIELDS + ('rotation_rate_yaw',)
    report['asset_values'] = {name: asset.get_editor_property(name) for name in fields}
    if world:
        movement = character.get_editor_property('character_movement')
        actual = {name: movement.get_editor_property(name) for name in MOVEMENT_FIELDS}
        actual.update({name: character.get_editor_property(name) for name in CHARACTER_FIELDS})
        actual['rotation_rate_yaw'] = movement.get_editor_property('rotation_rate').yaw
        report['runtime_values'] = actual
        for name, expected in report['asset_values'].items():
            if not math.isclose(actual[name], expected, rel_tol=1e-6, abs_tol=1e-6):
                report['errors'].append('Runtime movement value differs from DA: ' + name)
        report['runtime_values_match'] = not report['errors']
    return report


# Отчёт сохраняется и при исключении, чтобы ошибка не потерялась в консоли редактора.
output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / 'Verification/MovementDataAsset/latest-validation.json'
try:
    result = inspect()
except Exception as error:
    result = {'errors': [str(error)]}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
if result['errors']:
    raise RuntimeError('Movement settings validation failed: ' + '; '.join(result['errors']))
unreal.log('SANDS_MOVEMENT_SETTINGS_VALIDATION_OK ' + str(output))
