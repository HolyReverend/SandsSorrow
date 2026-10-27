"""Проверяет ресурсы прототипа этапа 1 или активный мир PIE.
Запуск: py ".../Scripts/Validate-Prototype.py" ".../report.json"
Компилирует заданный список Blueprint в памяти, не сохраняет и не перемещает ассеты.
Проверка свойств не подтверждает отрисовку и реальный ввод игрока.
"""
import json
import sys
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal

MAP = '/Game/Maps/TestMaps/L_LightPrototype'
CHARACTER = '/Game/Blueprints/Characters/BP_PlayerCharacter'
CONTROLLER = '/Game/Blueprints/Characters/BP_PlayerController'
GAME_MODE = '/Game/Blueprints/Core/BPGM_SandsOfSorrow'
ANIM = '/Game/Animations/ABP_PlayerLocomotion'
IMC = '/Game/Input/IMC_Player'
REFLECTOR = '/Game/Blueprints/Logic/Light/BP_Reflector'
CAMERA_MANAGER = '/Game/Blueprints/Characters/BP_PlayerCameraManager'
PROMPT = '/Game/Blueprints/UI/WBP_InteractionPrompt'


# Путь объекта для JSON; отсутствующие ссылки остаются null.
def ref(obj):
    return obj.get_path_name() if obj else None


# Преобразует вектор Unreal в обычные числовые поля отчёта.
def vector(value):
    return {k: float(getattr(value, k)) for k in ('x', 'y', 'z')}


# Собирает ошибки и снимок состояния: в PIE проверяет экземпляры, вне PIE — ресурсы и defaults.
def inspect():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game = editor.get_game_world()
    report = {'timestamp_utc': datetime.now(timezone.utc).isoformat(),
              'project': unreal.Paths.get_project_file_path(), 'engine': unreal.SystemLibrary.get_engine_version(),
              'pie': bool(game), 'errors': []}
    if game:
        # Фактическая цепочка владения и анимация должны принадлежать игровому Pawn.
        pc = unreal.GameplayStatics.get_player_controller(game, 0)
        pawn = pc.get_controlled_pawn() if pc else None
        gm = unreal.GameplayStatics.get_game_mode(game)
        if not (pc and pawn and gm):
            raise RuntimeError('Missing runtime GameMode/controller/Pawn')
        mesh = pawn.get_editor_property('mesh')
        anim = mesh.get_anim_instance()
        movement = pawn.get_editor_property('character_movement')
        report['runtime'] = {'world': ref(game), 'game_mode': ref(gm.get_class()),
            'controller': ref(pc.get_class()), 'pawn': ref(pawn.get_class()),
            'possessed': pawn.get_controller() == pc, 'location': vector(pawn.get_actor_location()),
            'velocity': vector(pawn.get_velocity()), 'control_rotation': str(pc.get_control_rotation()),
            'mesh': ref(mesh.get_skeletal_mesh_asset()), 'animation_class': ref(anim.get_class()) if anim else None}
        camera_manager = unreal.GameplayStatics.get_player_camera_manager(game, 0)
        report['runtime']['camera_manager'] = ref(camera_manager.get_class())
        report['runtime']['camera_location'] = vector(camera_manager.get_camera_location())
        expected = ((camera_manager, CAMERA_MANAGER), (gm, GAME_MODE), (pc, CONTROLLER), (pawn, CHARACTER))
        for obj, asset in expected:
            if ref(obj.get_class()) != asset + '.' + asset.rsplit('/', 1)[1] + '_C':
                report['errors'].append('Unexpected runtime class: ' + ref(obj.get_class()))
        if not report['runtime']['possessed']:
            report['errors'].append('Pawn not possessed')
        if not anim:
            report['errors'].append('Missing animation instance')
        else:
            report['animation'] = {k: str(anim.get_editor_property(k)) for k in
                                   ('Velocity', 'GroundSpeed', 'ShouldMove', 'IsFalling', 'Direction')}
            report['animation']['owner_matches'] = anim.get_editor_property('Character') == pawn
            report['animation']['movement_matches'] = anim.get_editor_property('MovementComponent') == movement
            if not (report['animation']['owner_matches'] and report['animation']['movement_matches']):
                report['errors'].append('AnimBP cached owner/movement does not match gameplay Pawn')
    else:
        # Статическая проверка требует открытую карту прототипа и не меняет её.
        world = editor.get_editor_world()
        report['world'] = ref(world)
        if ref(world) != MAP + '.L_LightPrototype':
            raise RuntimeError('Open L_LightPrototype before running the static prototype validator')
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        report['actors'] = len(actors)
        report['player_starts'] = [ref(a) for a in actors if isinstance(a, unreal.PlayerStart)]
        if len(report['player_starts']) != 1:
            report['errors'].append('Expected one PlayerStart')
        # Компиляция в памяти выявляет ошибки и неверное наследование без пересохранения пакетов.
        expected_parents = [(CHARACTER, '/Script/SandsOfSorrow.SandsOfSorrowPlayerCharacter'),
                            (CONTROLLER, '/Script/SandsOfSorrow.SandsOfSorrowGameplayPlayerController'),
                            (GAME_MODE, '/Script/Engine.GameModeBase'), (ANIM, '/Script/Engine.AnimInstance'),
                            (CAMERA_MANAGER, '/Script/Engine.PlayerCameraManager'),
                            (REFLECTOR, '/Script/SandsOfSorrow.SandsOfSorrowReflector'),
                            (PROMPT, '/Script/SandsOfSorrow.SandsOfSorrowInteractionPromptWidget')]
        report['blueprints'] = []
        for asset_path, parent in expected_parents:
            bp = unreal.load_asset(asset_path)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            row = {'asset': ref(bp), 'parent': ref(bp.get_blueprint_parent_class()),
                   'status': str(bp.get_editor_property('status'))}
            report['blueprints'].append(row)
            if row['parent'] != parent or bp.get_editor_property('status') != unreal.BlueprintStatus.BS_UP_TO_DATE:
                report['errors'].append('Blueprint parent/compile: ' + asset_path)
        # Проверяем связи карты, GameMode и классов персонажа, контроллера, камеры и UI.
        gm_class = unreal.load_asset(GAME_MODE).generated_class()
        char_class = unreal.load_asset(CHARACTER).generated_class()
        pc_class = unreal.load_asset(CONTROLLER).generated_class()
        gm = unreal.get_default_object(gm_class)
        char = unreal.get_default_object(char_class)
        pc = unreal.get_default_object(pc_class)
        if world.get_world_settings().get_editor_property('default_game_mode') != gm_class:
            report['errors'].append('Map GameMode override mismatch')
        if gm.get_editor_property('default_pawn_class') != char_class or gm.get_editor_property('player_controller_class') != pc_class:
            report['errors'].append('GameMode Pawn/controller mismatch')
        manager_class = unreal.load_asset(CAMERA_MANAGER).generated_class()
        if pc.get_editor_property('player_camera_manager_class') != manager_class:
            report['errors'].append('Controller camera manager mismatch')
        prompt_class = unreal.load_asset(PROMPT).generated_class()
        prompt_defaults = unreal.get_default_object(prompt_class)
        report['prompt_defaults'] = {'class': ref(prompt_class),
            'controller_class': ref(pc.get_editor_property('interaction_prompt_class')),
            'focusable': prompt_defaults.get_editor_property('is_focusable')}
        if pc.get_editor_property('interaction_prompt_class') != prompt_class or report['prompt_defaults']['focusable']:
            report['errors'].append('Prompt class assignment/focusability mismatch')
        manager = unreal.get_default_object(manager_class)
        boom = char.get_editor_property('camera_boom')
        report['camera'] = {'manager_class': ref(manager_class), 'pitch_min': manager.get_editor_property('view_pitch_min'),
            'pitch_max': manager.get_editor_property('view_pitch_max'), 'arm_length': boom.get_editor_property('target_arm_length'),
            'target_offset': vector(boom.get_editor_property('target_offset')), 'socket_offset': vector(boom.get_editor_property('socket_offset')),
            'collision_test': boom.get_editor_property('do_collision_test'), 'fov': char.get_editor_property('follow_camera').get_editor_property('field_of_view')}
        report['movement'] = {n: char.get_editor_property('character_movement').get_editor_property(n)
                              for n in ('max_walk_speed', 'max_acceleration', 'jump_z_velocity', 'air_control')}
        # Сверяем типы действий и привязки; фактические движение и осмотр требуют отдельного теста.
        actions = {n: unreal.load_asset('/Game/Input/Actions/IA_' + n) for n in ('Move', 'Look', 'Jump', 'Interact')}
        report['actions'] = {n: {'asset': ref(a), 'value_type': str(a.get_editor_property('value_type'))} for n, a in actions.items()}
        for n, action in actions.items():
            expected_type = unreal.InputActionValueType.AXIS2D if n in ('Move', 'Look') else unreal.InputActionValueType.BOOLEAN
            if action.get_editor_property('value_type') != expected_type:
                report['errors'].append('Input Action value type: ' + n)
        for field, name in [('move_action', 'Move'), ('look_action', 'Look'), ('jump_action', 'Jump'), ('interact_action', 'Interact')]:
            if char.get_editor_property(field) != actions[name]:
                report['errors'].append('Character action assignment: ' + name)
        context = unreal.load_asset(IMC)
        if pc.get_editor_property('player_mapping_context') != context:
            report['errors'].append('Controller context mismatch')
        mappings = context.get_editor_property('default_key_mappings').get_editor_property('mappings')
        report['mappings'] = [{'key': m.get_editor_property('key').export_text(),
            'action': ref(m.get_editor_property('action')), 'modifiers': [ref(x.get_class()) for x in m.get_editor_property('modifiers')]} for m in mappings]
        expected_keys = {'W': 'Move', 'A': 'Move', 'S': 'Move', 'D': 'Move', 'Mouse2D': 'Look', 'SpaceBar': 'Jump', 'E': 'Interact'}
        if len(mappings) != len(expected_keys) or {m['key'] for m in report['mappings']} != set(expected_keys):
            report['errors'].append('Unexpected key mappings')
        for mapping in report['mappings']:
            name = expected_keys.get(mapping['key'])
            if name and mapping['action'] != ref(actions[name]):
                report['errors'].append('Wrong action mapped to ' + mapping['key'])
        mesh = char.get_editor_property('mesh')
        report['mesh'] = ref(mesh.get_skeletal_mesh_asset())
        report['anim_class'] = ref(mesh.get_editor_property('anim_class'))
        if report['mesh'] != '/Game/Template/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple' or report['anim_class'] != ANIM + '.ABP_PlayerLocomotion_C':
            report['errors'].append('Temporary mesh/animation mismatch')
    # Общие требования к компоненту действуют и для defaults, и для экземпляра PIE.
    pawn_defaults = pawn if game else char
    interaction = pawn_defaults.get_interaction_component()
    if not interaction:
        report['errors'].append('Missing gameplay interaction component')
    else:
        report['interaction'] = {'component_class': ref(interaction.get_class()),
            'auto_activate': interaction.get_editor_property('auto_activate'), 'active': interaction.is_active(),
            'distance': interaction.get_editor_property('interaction_distance'),
            'camera_trace_distance': interaction.get_editor_property('camera_trace_distance'),
            'focus': ref(interaction.get_focused_actor()), 'text': str(interaction.get_focused_text()),
            'interact_action': ref(pawn_defaults.get_editor_property('interact_action'))}
        if not report['interaction']['auto_activate'] or (game and not report['interaction']['active']):
            report['errors'].append('Interaction component must auto-activate for gameplay')
        if report['interaction']['interact_action'] != '/Game/Input/Actions/IA_Interact.IA_Interact':
            report['errors'].append('Missing interaction action assignment')
    if game:
        # Проверяем единственность UI, владельца и соответствие текста/видимости авторитетному фокусу.
        prompt = pc.get_interaction_prompt()
        prompt_class = unreal.load_asset(PROMPT).generated_class()
        widgets = unreal.WidgetLibrary.get_all_widgets_of_class(game, prompt_class, top_level_only=True)
        available = pc.is_gameplay_interaction_available()
        shown = bool(available and interaction and interaction.get_focused_actor() and str(interaction.get_focused_text()))
        report['prompt'] = {'instance': ref(prompt), 'instances': [ref(w) for w in widgets],
            'gameplay_available': available, 'expected_shown': shown}
        if not prompt:
            report['errors'].append('Missing runtime interaction prompt')
        else:
            tree = next((o for o in unreal.ObjectIterator() if isinstance(o, unreal.WidgetTree) and o.get_outer() == prompt), None)
            label = unreal.find_object(tree, 'ActionText') if tree else None
            panel = unreal.find_object(tree, 'PromptPanel') if tree else None
            marker = unreal.find_object(tree, 'AimMarker') if tree else None
            report['prompt'].update({'owner_matches': prompt.get_owning_player() == pc,
                'in_viewport': prompt.is_in_viewport(), 'shown': prompt.is_prompt_shown(),
                'displayed_text': str(prompt.get_displayed_action_text()),
                'rendered_text': str(label.get_text()) if label else None,
                'panel_visible': panel.is_visible() if panel else None,
                'marker_visible': marker.is_visible() if marker else None})
            expected_text = str(interaction.get_focused_text()) if shown else ''
            if len(widgets) != 1 or widgets[0] != prompt or not prompt.is_in_viewport() or prompt.get_owning_player() != pc:
                report['errors'].append('Prompt ownership/count/viewport mismatch')
            if not (label and panel and marker):
                report['errors'].append('Missing BindWidget member')
            elif prompt.is_prompt_shown() != shown or str(label.get_text()) != expected_text or str(prompt.get_displayed_action_text()) != expected_text or panel.is_visible() != shown or marker.is_visible() != available:
                report['errors'].append('Prompt does not match the authoritative interaction focus')
            if not available and interaction and (interaction.get_focused_actor() or str(interaction.get_focused_text())):
                report['errors'].append('Focus must be empty when gameplay interaction is unavailable')
    # Число устройств и их состояние фиксируются без вызова взаимодействия.
    check_world = game if game else world
    reflector_class = unreal.load_class(None, '/Script/SandsOfSorrow.SandsOfSorrowReflector')
    reflectors = unreal.GameplayStatics.get_all_actors_of_class(check_world, reflector_class)
    report['reflectors'] = [{'actor': ref(a), 'class': ref(a.get_class()), 'index': a.get_rotation_index(),
        'normal': vector(a.get_face_normal()), 'location': vector(a.get_actor_location()),
        'enabled': a.get_editor_property('interaction_enabled')} for a in reflectors]
    if len(reflectors) != 2:
        report['errors'].append('Expected two prototype reflectors')
    report['dirty_content_packages'] = [ref(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    report['dirty_map_packages'] = [ref(p) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    return report


# Даже исключение записывается в отчёт; итоговая ошибка делает неуспех явным для запускающего инструмента.
output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(unreal.Paths.project_saved_dir()) / 'Verification/Prototype.json'
try:
    result = inspect()
except Exception:
    result = {'errors': [traceback.format_exc()]}
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
if result['errors']:
    raise RuntimeError('SANDS_PROTOTYPE_FAILED ' + str(output))
unreal.log('SANDS_PROTOTYPE_OK ' + str(output))
