"""Измеряет прямой разгон и остановку в PIE на временном ровном маршруте этапа 3."""

import builtins
import json
import time
from datetime import datetime, timezone
from pathlib import Path

import unreal


OUTPUT = Path(unreal.Paths.project_saved_dir()) / "Verification/Stage3/20261026_Step1/route-baseline.json"
EXPECTED_MAP = "/Game/Maps/TestMaps/UEDPIE_0_L_LightPrototype"
PRESS_SECONDS = 2.0
SETTLE_SECONDS = 1.0
DIRECTION = unreal.Vector(1.0, 0.0, 0.0)

previous = getattr(builtins, "_sands_stage3_route", None)
if previous and not previous["done"]:
    raise RuntimeError("A stage 3 route measurement is already active")


def xyz(value):
    return [round(float(getattr(value, axis)), 3) for axis in ("x", "y", "z")]


# Замер допустим только для запущенного игрового Pawn в явно выбранной тестовой комнате.
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world or not world.get_path_name().startswith(EXPECTED_MAP):
    raise RuntimeError("Start PIE on L_LightPrototype before the route measurement")
controller = unreal.GameplayStatics.get_player_controller(world, 0)
pawn = controller.get_controlled_pawn() if controller else None
if not pawn or pawn.get_class().get_path_name() != "/Game/Blueprints/Characters/BP_PlayerCharacter.BP_PlayerCharacter_C":
    raise RuntimeError("Expected a possessed BP_PlayerCharacter")

start = pawn.get_actor_location()
if abs(start.x + 1400.0) > 10.0 or abs(start.y + 250.0) > 10.0:
    raise RuntimeError("Unexpected route start: " + str(xyz(start)))

samples = []
started = time.monotonic()
state = {"handle": None, "done": False}


def measure_tick(delta_seconds):
    # Инъекция направления происходит каждый кадр; скорость и путь читает сам CharacterMovement.
    elapsed = time.monotonic() - started
    if elapsed < PRESS_SECONDS:
        pawn.add_movement_input(DIRECTION, 1.0)
    position = pawn.get_actor_location()
    velocity = pawn.get_velocity()
    samples.append({"t": round(elapsed, 5), "dt": round(float(delta_seconds), 5),
                    "location": xyz(position), "velocity": xyz(velocity),
                    "horizontal_speed": round((velocity.x ** 2 + velocity.y ** 2) ** 0.5, 3),
                    "phase": "press" if elapsed < PRESS_SECONDS else "release"})
    if elapsed < PRESS_SECONDS + SETTLE_SECONDS:
        return

    # Снимаем callback даже при неудачной записи: он не должен влиять на следующую PIE-сессию.
    unreal.unregister_slate_post_tick_callback(state["handle"])
    state["done"] = True
    result = {"timestamp_utc": datetime.now(timezone.utc).isoformat(),
              "mode": "rendered PIE, programmatic AddMovementInput; not physical keyboard input",
              "map": world.get_path_name(), "pawn": pawn.get_class().get_path_name(),
              "start": xyz(start), "direction": xyz(DIRECTION),
              "press_seconds": PRESS_SECONDS, "settle_seconds": SETTLE_SECONDS,
              "samples": samples}
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SANDS_STAGE3_ROUTE_OK " + str(OUTPUT))


def tick(delta_seconds):
    # Ошибка или досрочный выход из PIE должны снять callback, чтобы не портить следующую сессию.
    try:
        measure_tick(delta_seconds)
    except Exception as error:
        unreal.unregister_slate_post_tick_callback(state["handle"])
        state["done"] = True
        unreal.log_error("SANDS_STAGE3_ROUTE_ABORTED: " + str(error))


state["handle"] = unreal.register_slate_post_tick_callback(tick)
builtins._sands_stage3_route = state
unreal.log("SANDS_STAGE3_ROUTE_STARTED " + str(OUTPUT))
