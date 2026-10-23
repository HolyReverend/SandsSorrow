import unreal
import json
from pathlib import Path

root = Path(unreal.Paths.project_dir())
report_path = root / 'Saved/EnvironmentSetup/template-validation.json'
report = {'moves': [], 'blueprints': [], 'errors': []}
eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
old = '/Game/ThirdPerson'
new = '/Game/Template/ThirdPerson'
# Only operate on the remaining template folder; Unreal owns external actor packages.
remaining = [a for a in registry.get_assets_by_path(old, recursive=True) if str(a.asset_class_path.asset_name) != 'ObjectRedirector']
if remaining:
    raise RuntimeError('Resources outside Template; migration requires a separate reviewed operation')
if not eas.does_asset_exist(new + '/Lvl_ThirdPerson'):
    raise RuntimeError('Moved map not found')
registry.search_all_assets(True)
for data in registry.get_assets_by_path('/Game', recursive=True):
    kind = str(data.asset_class_path.asset_name)
    if data.is_redirector() or str(data.package_name).startswith(('/Game/__ExternalActors__/', '/Game/__ExternalObjects__/')):
        continue
    asset = data.get_asset()
    if not asset:
        report['errors'].append('Load failed: ' + str(data.package_name))
        continue
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        status = asset.get_editor_property('status')
        report['blueprints'].append({'asset': str(data.package_name), 'status': str(status)})
        if status != unreal.BlueprintStatus.BS_UP_TO_DATE:
            report['errors'].append('Blueprint status: ' + str(data.package_name) + ' ' + str(status))
report['outside_template'] = [str(a.package_name) for a in registry.get_assets_by_path('/Game', recursive=True) if str(a.asset_class_path.asset_name) != 'ObjectRedirector' and not str(a.package_name).startswith(('/Game/Template/', '/Game/__ExternalActors__/', '/Game/__ExternalObjects__/'))]
report['maps'] = []
level_system = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for name in ['/Game/Template/ThirdPerson/Lvl_ThirdPerson', '/Game/Template/Variant_Combat/Lvl_Combat', '/Game/Template/Variant_Platforming/Lvl_Platforming', '/Game/Template/Variant_SideScrolling/Lvl_SideScrolling']:
    loaded = level_system.load_level(name)
    count = len(actor_system.get_all_level_actors()) if loaded else 0
    starts = sum(1 for a in actor_system.get_all_level_actors() if isinstance(a, unreal.PlayerStart)) if loaded else 0
    report['maps'].append({'path':name, 'loaded':loaded, 'actors':count, 'player_starts':starts})
    if name.endswith('Lvl_ThirdPerson') and starts == 0:
        report['errors'].append('Main map has no PlayerStart')
    if not loaded or count == 0 or (name.endswith('Lvl_ThirdPerson') and count < 60):
        report['errors'].append('Map failed: '+name)
report_path.write_text(json.dumps(report, indent=2), encoding='utf-8')
if report['errors'] or report['outside_template']:
    raise RuntimeError('Template validation failed: ' + str(report_path))
unreal.log('SANDS_TEMPLATE_VALIDATION_OK ' + str(len(report['blueprints'])))
