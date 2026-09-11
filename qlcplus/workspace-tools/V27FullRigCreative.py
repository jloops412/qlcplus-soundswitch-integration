#!/usr/bin/env python3
"""Reviewed V27 programming for Wash FX Hex plus two Focus Spot Two units.

V26 IR-4/tube frames remain the musical source. Every raw loop and moving
Priority look has an explicit recipe. Focus A/B means source instance order,
not proven physical left/right placement. The nine position pairs are exact
16-bit values recovered from the shared SoundSwitch project, but installed
aim/inversion, focus endpoints, and Wash zone order still require a bench.
"""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from typing import Any
import xml.etree.ElementTree as ET


PROGRAM_COMPLETE = True
PROGRAM_VERSION = "v27-full-rig-creative-2026-08-29.2"
POSITION_CALIBRATION_PROVENANCE = "decoded SoundSwitch Focus A/B presets"
POSITION_BENCH_REQUIRED = True
WASH_ZONE_ORIENTATION_BENCH_REQUIRED = True
ENABLE_FOCUS_UV = False

IR4_PHYSICAL = (0, 1, 2, 3)
TUBE_PHYSICAL = (5, 6, 7, 8)
IR4_PRIVATE = (100, 101, 102, 103)
TUBE_PRIVATE = (105, 106, 107, 108)
WASH_PHYSICAL, FOCUS_A_PHYSICAL, FOCUS_B_PHYSICAL = 4, 9, 10
WASH_PRIVATE, FOCUS_A_PRIVATE, FOCUS_B_PRIVATE = 104, 109, 110

FOCUS_HARD = 0
# Focus direction is undocumented; this soft endpoint is bench-tunable.
FOCUS_SOFT = 96
FOCUS_BANK_GAIN_CAP = {
    "medium": (0.45, 115), "colorful": (0.55, 140),
    "slow": (0.32, 82), "flashy": (0.67, 170), "priority": (0.35, 90),
}
FOCUS_PT_SPEED = {
    "medium": 150, "colorful": 110, "slow": 210, "flashy": 40, "priority": 220,
}
WASH_BANK_GAIN_CAP = {
    "medium": (0.60, 153), "colorful": (0.75, 191),
    "slow": (0.42, 107), "flashy": (0.82, 209), "priority": (0.45, 115),
}
WASH_UV_CAP = {"medium": 80, "colorful": 96, "slow": 64, "flashy": 110, "priority": 72}
WASH_MASK_FLOOR = {"medium": .15, "colorful": .10, "slow": .55, "flashy": 0., "priority": .25}

# (A pan16, A tilt16, B pan16, B tilt16), exact decoded source presets.
SOURCE_POSITION_PRESETS: dict[str, tuple[int, int, int, int]] = {
    "CROSS_OUT_DOWN": (46027, 8286, 40693, 6629),
    "CROSS_IN_DOWN": (50752, 6930, 53342, 3917),
    "STAGE_RIGHT": (35663, 7231, 33682, 3917),
    "STAGE_LEFT": (20727, 65384, 23013, 63878),
    "STRAIGHT_AHEAD": (39473, 2260, 47094, 1055),
    "CROSS_OUT_UP": (40693, 2561, 45875, 1356),
    "CROSS_IN_UP": (39626, 3314, 47094, 1356),
    "UP": (45417, 15066, 40997, 13107),
    "DOWN": (50142, 8587, 49532, 6629),
}
POSITION_SCENES = (
    (2175, "Crossed Out Down", "CROSS_OUT_DOWN"),
    (2176, "Crossed In Down", "CROSS_IN_DOWN"),
    (2177, "Stage Right", "STAGE_RIGHT"),
    (2178, "Stage Left", "STAGE_LEFT"),
    (2179, "Straight Ahead", "STRAIGHT_AHEAD"),
    (2180, "Crossed Out Up", "CROSS_OUT_UP"),
    (2181, "Crossed In Up", "CROSS_IN_UP"),
    (2182, "Up", "UP"),
    (2183, "Down", "DOWN"),
)
POSITION_ALIAS = {
    "PK": "UP", "SA": "STRAIGHT_AHEAD", "CI": "CROSS_IN_UP",
    "CO": "CROSS_OUT_UP", "SL": "STAGE_LEFT", "SR": "STAGE_RIGHT",
    "LR": "STAGE_LEFT", "RL": "STAGE_RIGHT", "DB": "UP", "AN": "STRAIGHT_AHEAD",
}

MOVEMENT_SEQUENCES: dict[str, tuple[str, ...]] = {
    "HOLD": ("CI",) * 8,
    "DRIFT": ("CI", "CI", "SA", "CO", "CO", "SA", "CI", "CI"),
    "WAVE": ("SL", "CI", "SA", "SR", "CO", "SR", "SA", "CI"),
    "SWEEP": ("SL", "SL", "CI", "SA", "SR", "SR", "CO", "SA"),
    "SWAP": ("CI", "CI", "CO", "CO", "CI", "CI", "CO", "CO"),
    "BOUNCE": ("CO", "SA", "CI", "SA", "CO", "SA", "CI", "SA"),
    "CENOUT": ("CI", "CI", "SA", "CO", "CO", "SA", "CI", "CI"),
    "OUTIN": ("CI", "CI", "SA", "CO", "CO", "SA", "CI", "CI")[::-1],
    "ROLLF": ("SL", "CI", "SA", "SR", "SR", "SA", "CI", "SL"),
    "ROLLR": ("SL", "CI", "SA", "SR", "SR", "SA", "CI", "SL")[::-1],
    "COMETF": ("SL", "CI", "SA", "SR", "SR", "SA", "CI", "SL"),
    "COMETR": ("SL", "CI", "SA", "SR", "SR", "SA", "CI", "SL")[::-1],
    "WALTZ": ("SL", "CI", "SA", "SR", "CO", "SR", "SA", "CI"),
    "ALT": ("LR", "LR", "RL", "RL", "LR", "LR", "RL", "RL"),
    "MONTAGE": ("CI", "LR", "CO", "RL", "SL", "SR", "DB", "CI"),
    "BUILD": ("PK", "CI", "CI", "SA", "SA", "CO", "CO", "CO"),
    "DROP": ("SA", "SA", "CI", "CI", "PK", "CO", "CO", "CI"),
    "HIT_EVEN": ("CI",) * 8, "HIT_ALT": ("LR",) * 8, "HIT_TRIPLET": ("CO",) * 8,
    "HIT_MOVE": ("CI", "CO", "CO", "CI", "CI", "CO", "CO", "CI"),
    "BUILD_HIT": ("PK", "CI", "CI", "SA", "SA", "CO", "CO", "CO"),
    "DROP_HIT": ("SA", "SA", "CI", "CI", "PK", "CO", "CO", "CI"),
    "ALT_HEAD": ("LR",) * 8,
    "ALT_STAGE": ("LR", "RL", "RL", "LR", "LR", "RL", "RL", "LR"),
    "PUNCH_C": ("CI",) * 8, "PUNCH_O": ("CO",) * 8,
    "APERTURE": ("CI", "CO", "CO", "CI", "CI", "CO", "CO", "CI"),
    "COMET_HIT": ("LR",) * 8, "COMETR_HIT": ("RL",) * 8,
    "HIT_PAIR": ("CI",) * 8, "DROP_C": ("CI",) * 8, "DROP_O": ("CO",) * 8,
    "FINALE": ("CI", "LR", "CO", "RL", "SL", "SR", "DB", "CI"),
}

# id: movement, Wash mask, optics, wheel treatment.
LOOP_PROGRAM: dict[int, tuple[str, str, str, str]] = {
532:("DRIFT","FULL","OPEN","NEAR"),533:("DRIFT","FULL","SOFT","NEAR"),534:("DRIFT","FULL","OPEN","NEAR"),535:("WAVE","DIAG","G3ROT","NEAR"),
536:("SWAP","CHECK","G1","NEAR"),537:("SWAP","CHECK","G2","NEAR"),538:("WAVE","ROLLF","OPEN","NEAR"),539:("DRIFT","FULL","OPEN","NEAR"),
540:("ROLLF","ROLLF","G1","NEAR"),541:("BOUNCE","BOUNCE","OPEN","NEAR"),542:("BOUNCE","DIAG","G2","NEAR"),543:("SWAP","HALVES","OPEN","NEAR"),
544:("OUTIN","OUTIN","OPEN","NEAR"),545:("CENOUT","CENOUT","OPEN","NEAR"),546:("ALT","HALVES","G1","PALETTE"),547:("ROLLF","ROLLF","OPEN","PALETTE"),
548:("ALT","CHECK","G1","NEAR"),549:("ALT","CHECK","G2","NEAR"),550:("COMETF","COMETF","G1ROT","NEAR"),551:("COMETF","COMETF","G2PRISM","NEAR"),
552:("MONTAGE","CORNERS","G1G2","NEAR"),553:("DRIFT","FULL","OPEN","ROT_SLOW"),554:("WAVE","ROLLF","G1","NEAR"),555:("WAVE","ROLLR","G2","NEAR"),
556:("HOLD","SPARKLE","OPEN","NEAR"),557:("ROLLF","ROLLF","G3UV","NEAR"),558:("ROLLF","ROLLF","G1","NEAR"),559:("SWAP","HALVES","G2","NEAR"),
560:("BUILD","BUILD","OPEN","NEAR"),561:("DROP","DROP","SOFT","NEAR"),562:("WAVE","CHECK","G2","NEAR"),563:("MONTAGE","MONTAGE","G3PRISM","PALETTE"),
564:("ROLLF","ROLLF","G1","PALETTE"),565:("SWEEP","ROLLF","PRISM","ROT_SLOW"),566:("BOUNCE","BOUNCE","PRISM","ROT_SLOW"),567:("ROLLF","CHECK","G2ROT","PALETTE"),
568:("WAVE","DIAG","G1","NEAR"),569:("ALT","CHECK","G2","NEAR"),570:("ROLLF","ROLLF","OPEN","PALETTE"),571:("ALT","HALVES","G1","NEAR"),
572:("WAVE","CORNERS","G2PRISM","PALETTE"),573:("ROLLF","CENOUT","G1","NEAR"),574:("BOUNCE","MONTAGE","G2","NEAR"),575:("WAVE","ROLLR","SOFT","NEAR"),
576:("OUTIN","BUILD","G3","NEAR"),577:("WAVE","SPARKLE","G2PRISM","PALETTE"),578:("DRIFT","FULL","SOFT","PALETTE_LIGHT"),579:("SWAP","DIAG","G1","NEAR"),
580:("WAVE","DIAG","G2","NEAR"),581:("ALT","HALVES","G1SHUT","NEAR"),582:("ROLLF","BUILD","G1","PALETTE"),583:("OUTIN","DROP","G2","PALETTE"),
584:("ROLLF","ROLLF","OPEN","PALETTE"),585:("ROLLR","ROLLR","OPEN","PALETTE"),586:("HOLD","SPARKLE","G1PRISM","NEAR"),587:("WALTZ","SPARKLE","G2","NEAR"),
588:("MONTAGE","CHECK","G3PRISM","ROT_MED"),589:("WALTZ","CORNERS","G2","NEAR"),590:("WAVE","HALVES","G3","NEAR"),591:("DRIFT","FULL","SOFT","NEAR"),
592:("ROLLF","ROLLF","G1","NEAR"),593:("OUTIN","CHECK","G3","NEAR"),594:("BUILD","BUILD","G2","PALETTE"),595:("DROP","DROP","G3PRISM","ROT_MED"),
596:("HOLD","FULL","SOFT","NEAR"),597:("DRIFT","FULL","SOFT","NEAR"),598:("DRIFT","FULL","G2SOFT","NEAR"),599:("SWAP","HALVES","OPEN","NEAR"),
600:("DRIFT","FULL","SOFT","NEAR"),601:("DRIFT","FULL","SOFT","NEAR"),602:("WALTZ","FULL","SOFT","PALETTE_LIGHT"),603:("HOLD","SPARKLE","G1SOFT","NEAR"),
604:("DRIFT","HALVES","SOFT","NEAR"),605:("DRIFT","FULL","SOFT","NEAR"),606:("HOLD","FULL","SOFT","NEAR"),607:("DRIFT","SPARKLE","G1PRISM","NEAR"),
608:("WAVE","ROLLF","SOFT","NEAR"),609:("WAVE","ROLLR","SOFT","NEAR"),610:("CENOUT","CENOUT","SOFT","NEAR"),611:("OUTIN","OUTIN","SOFT","NEAR"),
612:("WAVE","ROLLF","SOFT","NEAR"),613:("WAVE","BUILD","SOFT","NEAR"),614:("DRIFT","FULL","SOFT","NEAR"),615:("HOLD","FULL","OPEN","NEAR"),
616:("WAVE","ROLLF","G1SOFT","NEAR"),617:("WAVE","ROLLR","G1SOFT","NEAR"),618:("CENOUT","CENOUT","SOFT","NEAR"),619:("OUTIN","OUTIN","SOFT","NEAR"),
620:("WALTZ","FULL","G1SOFT","NEAR"),621:("WALTZ","SPARKLE","OPENPRISM","NEAR"),622:("ALT","CHECK","G1SOFT","NEAR"),623:("ALT","HALVES","G2SOFT","NEAR"),
624:("BUILD","BUILD","SOFT","NEAR"),625:("DRIFT","FULL","SOFT","ROT_VSLOW"),626:("WALTZ","FULL","SOFT","NEAR"),627:("HOLD","FULL","SOFT","NEAR"),
628:("HIT_EVEN","HIT","OPEN","WHITE"),629:("HIT_ALT","HIT","OPEN","NEAR"),630:("HIT_ALT","MONTAGE","OPEN","PALETTE"),631:("HOLD","SPARKLE","G1PRISM","WHITE"),
632:("HIT_TRIPLET","HIT","UV","NEAR"),633:("HIT_MOVE","CENOUT","G1PRISM","NEAR"),634:("BUILD_HIT","BUILD","OPEN","NEAR"),635:("DROP_HIT","DROP","PRISM","NEAR"),
636:("ALT_HEAD","ROLLF","G1","NEAR"),637:("ALT_HEAD","ROLLR","G1","NEAR"),638:("ALT_HEAD","ROLLF","OPEN","PALETTE"),639:("PUNCH_C","CENOUT","OPEN","WHITE"),
640:("PUNCH_O","OUTIN","OPEN","WHITE"),641:("ALT_HEAD","CHECK","OPEN","WHITE"),642:("ALT_STAGE","HALVES","OPEN","WHITE"),643:("HIT_ALT","MONTAGE","OPEN","PATRIOT"),
644:("HIT_ALT","CHECK","G1","NEAR"),645:("HIT_TRIPLET","HIT","G2PRISM","NEAR"),646:("ALT_HEAD","HIT","OPEN","RGB"),647:("APERTURE","CENOUT","G3","CMY"),
648:("HOLD","SPARKLE","G1PRISM","ROT_MED"),649:("HIT_MOVE","BOUNCE","G2","NEAR"),650:("COMET_HIT","COMETF","G1PRISM","NEAR"),651:("COMETR_HIT","COMETR","G1PRISM","NEAR"),
652:("HOLD","ROLLF","OPEN","ROT_FAST"),653:("HOLD","ROLLR","OPEN","ROT_MED"),654:("HIT_PAIR","FULL","OPEN","PALETTE"),655:("ALT_HEAD","HIT","OPEN","NEAR"),
656:("BUILD_HIT","BUILD","G1PRISM","NEAR"),657:("DROP_C","CENOUT","G2PRISM","NEAR"),658:("DROP_O","OUTIN","G2PRISM","NEAR"),659:("FINALE","MONTAGE","G3PRISM","ROT_MED"),
}

# id: movement, mask, optics, wheel, Wash cap, Focus cap.
PRIORITY_PROGRAM: dict[int, tuple[str, str, str, str, int, int]] = {
12:("DRIFT","FULL","OPEN","NEAR",110,75),20:("DRIFT","FULL","UV","NEAR",100,72),
21:("WALTZ","FULL","SOFT","NEAR",105,75),29:("HOLD","SPARKLE","G1SOFT","NEAR",90,60),
30:("DRIFT","FULL","G2SOFT","NEAR",95,70),32:("DRIFT","SPARKLE","G1PRISM","NEAR",100,80),
33:("SWAP","HALVES","OPEN","NEAR",105,85),34:("WALTZ","CORNERS","G2PRISM","PALETTE_LIGHT",110,90),
35:("WAVE","CHECK","G3ROT","ROT_SLOW",115,110),36:("DRIFT","FULL","G1PRISM","ROT_VSLOW",115,100),
}

FOCUS_WHEEL = (
    (0,(1.,1.,1.)),(22,(1.,0.,0.)),(37,(0.,0.,1.)),(52,(0.,1.,0.)),
    (67,(1.,.92,0.)),(82,(1.,.12,.55)),(97,(.31,.74,1.)),
    (112,(.49,1.,.49)),(124,(1.,.86,.49)),
)
WHEEL_SEQUENCES = {
    "PALETTE":(22,67,52,97,37,82,124,0),
    "PALETTE_LIGHT":(124,112,97,82,0,97,112,124),
    "PATRIOT_A":(22,0,37,0,22,0,37,0), "PATRIOT_B":(37,0,22,0,37,0,22,0),
    "RGB":(22,52,37,22,52,37,0,0), "CMY":(97,82,67,97,82,67,0,0),
}
ROTATION_WHEEL = {"ROT_VSLOW":(187,196),"ROT_SLOW":(180,204),"ROT_MED":(160,224),"ROT_FAST":(140,244)}


def _clamp(value: float | int, low: int = 0, high: int = 255) -> int:
    return max(low, min(high, int(round(value))))


def _root_use(context: Mapping[str, Any]) -> Mapping[str, Any]:
    uses = context.get("uses", [])
    return uses[0] if uses else {
        "root_id": context["scene_id"], "root_name": context.get("scene_name", ""), "step_index": 0,
    }


def _root_id(context: Mapping[str, Any]) -> int:
    return int(_root_use(context).get("root_id", context["scene_id"]))


def _root_name(context: Mapping[str, Any]) -> str:
    return str(_root_use(context).get("root_name", context.get("scene_name", ""))).upper()


def _step(context: Mapping[str, Any]) -> int:
    raw = _root_use(context).get("step_index")
    return 0 if raw is None else int(raw) % 8


def _bank(context: Mapping[str, Any]) -> str:
    if context["category"] == "priority":
        return "priority"
    root = _root_id(context)
    if root <= 563: return "medium"
    if root <= 595: return "colorful"
    if root <= 627: return "slow"
    return "flashy"


def _source(context: Mapping[str, Any]) -> Mapping[Any, Any]:
    return context.get("source_fixture_values", {})


def _fixture_values(source: Mapping[Any, Any], fixture_id: int) -> Mapping[Any, Any]:
    return source.get(fixture_id, source.get(str(fixture_id), {}))


def _value(values: Mapping[Any, Any], channel: int) -> int:
    return int(values.get(channel, values.get(str(channel), 0)))


def _layer_ids(context: Mapping[str, Any]) -> tuple[tuple[int, ...], tuple[int, ...]]:
    return (IR4_PRIVATE, TUBE_PRIVATE) if context["category"] == "priority" else (IR4_PHYSICAL, TUBE_PHYSICAL)


def _ir_rgba_w_uv(values: Mapping[Any, Any]) -> tuple[float, ...]:
    master = _value(values, 0) / 255.
    return (
        _value(values, 1) * master, _value(values, 2) * master,
        _value(values, 3) * master, _value(values, 5) * master,
        _value(values, 4) * master, _value(values, 6) * master,
    )


def _tube_rgba_w_uv(values: Mapping[Any, Any], pixel: int) -> tuple[float, ...]:
    base = pixel * 5
    return (
        float(_value(values, base)), float(_value(values, base + 1)),
        float(_value(values, base + 2)), float(_value(values, base + 4)),
        float(_value(values, base + 3)), 0.,
    )


def _percentile(values: Sequence[float], percentile: float) -> float:
    if not values: return 0.
    ordered = sorted(values)
    rank = (len(ordered) - 1) * percentile
    low, high = int(rank), min(int(rank) + 1, len(ordered) - 1)
    fraction = rank - low
    return ordered[low] * (1. - fraction) + ordered[high] * fraction


def _blend(values: Sequence[float]) -> float:
    return 0. if not values else .65 * (sum(values) / len(values)) + .35 * max(values)


def _program(context: Mapping[str, Any]) -> tuple[str, str, str, str]:
    root = _root_id(context)
    if context["category"] == "raw":
        if root not in LOOP_PROGRAM: raise ValueError(f"No reviewed recipe for raw loop {root}.")
        return LOOP_PROGRAM[root]
    return PRIORITY_PROGRAM.get(root, ("HOLD","FULL","OPEN","NEAR",115,90))[:4]


def _snake(sequence: Sequence[int], step: int, floor: float, comet: bool = False) -> list[float]:
    result = [floor] * 6
    weights = (1., .55, .25) if comet else (1., .45, .20)
    for back, weight in enumerate(weights):
        zone = sequence[(step - back) % 8]
        result[zone] = max(result[zone], weight)
    return result


def _wash_mask(name: str, step: int, floor: float) -> list[float]:
    if name == "FULL": return [1.] * 6
    if name == "CHECK": return list(((1.,.2,1.,.2,1.,.2),(.2,1.,.2,1.,.2,1.))[step % 2])
    if name == "HALVES": return list(((1.,.45,.15,1.,.45,.15),(.15,.45,1.,.15,.45,1.))[(step // 2) % 2])
    if name == "DIAG": return list(((1.,.5,.15,.15,.5,1.),(.15,.5,1.,1.,.5,.15))[step % 2])
    if name == "ROLLF": return _snake((0,1,2,5,4,3,0,1), step, floor)
    if name == "ROLLR": return _snake((1,0,3,4,5,2,1,0), step, floor)
    if name == "COMETF": return _snake((0,1,2,5,4,3,0,1), step, floor, True)
    if name == "COMETR": return _snake((1,0,3,4,5,2,1,0), step, floor, True)
    if name == "BOUNCE":
        result = [floor] * 6; result[(0,1,2,1,0,3,4,5)[step]] = 1.; return result
    if name == "CORNERS":
        pairs = ((0,5),(2,3),(0,2),(3,5),(3,5),(0,2),(2,3),(0,5))
        result = [floor] * 6
        for zone in pairs[step]: result[zone] = 1.
        return result
    if name in {"CENOUT","OUTIN"}:
        radius = (.1,.25,.5,.75,1.,.75,.5,.25)
        scalar = radius[step if name == "CENOUT" else 7 - step]
        return [max(floor,scalar),1.,max(floor,scalar),max(floor,scalar),1.,max(floor,scalar)]
    if name == "SPARKLE":
        result = [floor] * 6; result[(0,5,1,4,2,3,1,4)[step]] = 1.; return result
    if name == "BUILD":
        count = max(1, (6 * (step + 1) + 7) // 8)
        return [1. if zone < count else floor for zone in range(6)]
    if name == "DROP": return [( .3,.45,.6,.8,0.,0.,1.,.35)[step]] * 6
    if name == "HIT": return [1. if step % 2 == 0 else 0.] * 6
    if name == "MONTAGE":
        names = ("FULL","CHECK","DIAG","HALVES","CHECK","DIAG","HALVES","FULL")
        return _wash_mask(names[step], step + (1 if 4 <= step <= 6 else 0), floor)
    raise ValueError(f"Unknown Wash mask {name}.")


def _wash_from_source(context: Mapping[str, Any]) -> list[int]:
    source = _source(context); ir_ids, tube_ids = _layer_ids(context)
    ir = [_ir_rgba_w_uv(_fixture_values(source, fixture_id)) for fixture_id in ir_ids]
    pixels = [_tube_rgba_w_uv(_fixture_values(source, fixture_id), pixel)
              for fixture_id in tube_ids for pixel in range(8)]
    bins = ((0,5),(5,10),(10,16),(16,21),(21,26),(26,32)); nearest = (0,1,1,2,2,3)
    _movement, mask_name, _optics, _wheel = _program(context)
    bank = _bank(context); gain, cap = WASH_BANK_GAIN_CAP[bank]
    if context["category"] == "priority": cap = min(cap, PRIORITY_PROGRAM.get(_root_id(context),("","","","",115,90))[4])
    floor = .65 if context["category"] == "priority" and _root_id(context) == 29 else WASH_MASK_FLOOR[bank]
    mask = _wash_mask(mask_name, _step(context), floor); frame = [0] * 40
    for zone, (start, end) in enumerate(bins):
        for component in range(6):
            values = [pixels[index][component] for index in range(start, end)]
            tube = .65 * (sum(values) / len(values)) + .35 * max(values)
            raw = .8 * tube + .2 * ir[nearest[zone]][component]
            value = _clamp(min(cap, raw * gain * mask[zone]))
            if component == 5: value = min(value, WASH_UV_CAP[bank])
            frame[4 + zone * 6 + component] = value
    return frame


def _focus_aggregate(context: Mapping[str, Any], side: int) -> tuple[float, ...]:
    source = _source(context); ir_ids, tube_ids = _layer_ids(context)
    samples = [_ir_rgba_w_uv(_fixture_values(source, fixture_id))
               for fixture_id in (ir_ids[:2] if side == 0 else ir_ids[2:])]
    for fixture_id in (tube_ids[:2] if side == 0 else tube_ids[2:]):
        values = _fixture_values(source, fixture_id)
        samples.extend(_tube_rgba_w_uv(values, pixel) for pixel in range(8))
    components = tuple(_blend([sample[index] for sample in samples]) for index in range(6))
    return (*components, _percentile([max(sample) for sample in samples], .75))


def _nearest_wheel(red: float, green: float, blue: float, amber: float, white: float, uv: float) -> int:
    rgb = (red+white+amber+.35*uv, green+white+.35*amber, blue+white+.8*uv)
    peak = max(rgb)
    if peak <= 8: return 0
    normalized = tuple(value / peak for value in rgb)
    if max(normalized) - min(normalized) <= .12: return 0
    return min(FOCUS_WHEEL, key=lambda item: sum((normalized[i]-item[1][i])**2 for i in range(3)))[0]


def _semantic_flash_wheel(name: str, side: int) -> int:
    if "RED / BLUE" in name: return (22,37)[side]
    if "RED FIXTURE" in name: return 22
    if "BLUE FIXTURE" in name: return 37
    if "TEAL MAGENTA" in name: return (97,82)[side]
    if "PURPLE GOLD" in name: return (82,67)[side]
    if "UV" in name: return (82,37)[side]
    if "GOLD" in name: return 67
    if "WHITE" in name: return 0
    if "COMET" in name: return (67,82)[side]
    return 0


def _focus_gate(movement: str, optics: str, step: int, side: int) -> float:
    if movement in {"HIT_ALT","ALT_HEAD","ALT_STAGE"}: return 1. if step % 2 == side else 0.
    if movement in {"COMETF","COMET_HIT","COMETR","COMETR_HIT"}:
        seq = (1.,.7,.4,.15,.15,.4,.7,1.)
        if movement in {"COMETR","COMETR_HIT"}: seq = seq[::-1]
        return seq[(step - side * 4) % 8]
    if optics == "G1SHUT": return 1. if step % 2 == side else 0.
    gates = {
        "BUILD":(0.,.15,.3,.45,.6,.75,.9,1.), "DROP":(.3,.45,.6,.8,0.,0.,1.,.35),
        "HIT_EVEN":(1.,0.,1.,0.,1.,0.,1.,0.), "HIT_TRIPLET":(1.,0.,1.,1.,0.,1.,1.,0.),
        "HIT_MOVE":(1.,0.,1.,0.,1.,0.,1.,0.), "BUILD_HIT":(.15,.25,.4,.55,.7,.85,1.,.25),
        "DROP_HIT":(.25,.4,.6,.8,0.,0.,1.,.35),
        "PUNCH_C":(.15,.35,.65,1.,1.,.65,.35,.15), "PUNCH_O":(.15,.35,.65,1.,1.,.65,.35,.15),
        "APERTURE":(1.,0.,1.,0.,1.,0.,1.,0.), "HIT_PAIR":(1.,1.,0.,0.,1.,1.,0.,0.),
        "DROP_C":(.25,.4,.6,.8,0.,0.,1.,.35), "DROP_O":(.25,.4,.6,.8,0.,0.,1.,.35),
        "FINALE":(1.,0.,1.,0.,1.,0.,1.,1.),
    }
    return gates.get(movement, (1.,) * 8)[step]


def _held_on_dark(desired: Sequence[Any], gates: Sequence[float], step: int) -> Any:
    state = desired[0]
    for absolute in range(16 + step + 1):
        index = absolute % 8
        if gates[index] <= 0.: state = desired[index]
    return state


def _split16(value: int) -> tuple[int, int]:
    return (value >> 8) & 255, value & 255


def _position(movement: str, optics: str, step: int, side: int, bank: str) -> tuple[int, int]:
    sequence = MOVEMENT_SEQUENCES[movement]
    alias = sequence[step]
    if bank == "flashy":
        gates = tuple(_focus_gate(movement, optics, index, side) for index in range(8))
        alias = _held_on_dark(sequence, gates, step)
    preset = SOURCE_POSITION_PRESETS[POSITION_ALIAS[alias]]
    return (preset[0],preset[1]) if side == 0 else (preset[2],preset[3])


def _wheel_value(token: str, context: Mapping[str, Any], movement: str, optics: str,
                 side: int, aggregate: tuple[float, ...]) -> int:
    step, bank = _step(context), _bank(context)
    if token == "WHITE": return 0
    if token in ROTATION_WHEEL: return ROTATION_WHEEL[token][side]
    if token == "NEAR":
        return _semantic_flash_wheel(_root_name(context), side) if bank == "flashy" else _nearest_wheel(*aggregate[:6])
    if token == "PATRIOT": desired = WHEEL_SEQUENCES["PATRIOT_A" if side == 0 else "PATRIOT_B"]
    elif token in {"PALETTE","PALETTE_LIGHT","RGB","CMY"}:
        source = WHEEL_SEQUENCES[token]; offset = side * (3 if token in {"RGB","CMY"} else 4)
        desired = tuple(source[(index + offset) % 8] for index in range(8))
    else: raise ValueError(f"Unknown wheel treatment {token}.")
    if bank == "flashy":
        gates = tuple(_focus_gate(movement, optics, index, side) for index in range(8))
        return int(_held_on_dark(desired, gates, step))
    return int(desired[step])


def _optics(token: str, context: Mapping[str, Any], side: int) -> tuple[int,int,int,int]:
    gobo = 14 if token.startswith("G1") else 23 if token.startswith("G2") else 32 if token.startswith("G3") else 0
    if token == "G1G2": gobo = (14,23)[side]
    rotation = (185,204)[side] if "ROT" in token and token.startswith(("G1","G2")) else (178,214)[side] if "ROT" in token else 0
    prism = 0
    if "PRISM" in token:
        if token == "PRISM" or "PRISM" in _root_name(context) or _bank(context) == "flashy" or _step(context) in {3,7}: prism = 16
    focus = FOCUS_SOFT if "SOFT" in token else FOCUS_HARD
    return gobo, rotation, prism, focus


def _focus_from_source(context: Mapping[str, Any], side: int) -> list[int]:
    movement, _wash, optics, wheel = _program(context); bank = _bank(context); step = _step(context)
    aggregate = _focus_aggregate(context, side); gain, cap = FOCUS_BANK_GAIN_CAP[bank]
    if context["category"] == "priority": cap = min(cap, PRIORITY_PROGRAM.get(_root_id(context),("","","","",115,90))[5])
    gate = _focus_gate(movement, optics, step, side)
    dimmer = _clamp(min(cap, aggregate[6] * gain) * gate); shutter = 8 if dimmer else 0
    pan16, tilt16 = _position(movement, optics, step, side, bank)
    pan, pan_fine = _split16(pan16); tilt, tilt_fine = _split16(tilt16)
    colour = _wheel_value(wheel, context, movement, optics, side, aggregate)
    if optics in {"UV","G3UV"}: colour = (82,37)[side]
    gobo, rotation, prism, focus = _optics(optics, context, side)
    uv_shutter = uv_dimmer = 0
    if ENABLE_FOCUS_UV and optics in {"UV","G3UV"}:
        uv_shutter, uv_dimmer = 8, min(32 if bank in {"slow","priority"} else 48, dimmer)
    return [pan,pan_fine,tilt,tilt_fine,colour,gobo,rotation,prism,shutter,dimmer,
            uv_shutter,uv_dimmer,focus,0,0,0,FOCUS_PT_SPEED[bank],0]


def _focus_fixed(position: str, side: int, colour: int, dimmer: int, speed: int = 220) -> list[int]:
    preset = SOURCE_POSITION_PRESETS[POSITION_ALIAS[position]]
    pan16, tilt16 = (preset[0],preset[1]) if side == 0 else (preset[2],preset[3])
    pan, pan_fine = _split16(pan16); tilt, tilt_fine = _split16(tilt16)
    return [pan,pan_fine,tilt,tilt_fine,colour,0,0,0,8 if dimmer else 0,dimmer,0,0,FOCUS_HARD,0,0,0,speed,0]


def _wash_fixed(zones: Sequence[Sequence[int]]) -> list[int]:
    if len(zones) != 6: raise ValueError("Wash frame requires six zones.")
    frame = [0] * 40
    for zone, colour in enumerate(zones):
        if len(colour) != 6: raise ValueError("Wash zone requires RGBAWUV.")
        for component, value in enumerate(colour): frame[4 + zone * 6 + component] = _clamp(value)
    return frame


def _tube_frame(pixels: Sequence[Sequence[int]]) -> list[int]:
    if len(pixels) != 8: raise ValueError("Tube frame requires eight pixels.")
    frame = [0] * 40
    for pixel, colour in enumerate(pixels):
        if len(colour) != 5: raise ValueError("Tube pixel requires RGBWA.")
        for component, value in enumerate(colour): frame[pixel * 5 + component] = _clamp(value)
    return frame


def _solid_tube(colour: Sequence[int]) -> list[int]:
    return _tube_frame([tuple(colour)] * 8)


def _performance(context: Mapping[str, Any]) -> dict[int, Any]:
    scene_id = int(context["scene_id"]); result: dict[int, Any] = {}
    if scene_id == 0:
        for fixture_id in TUBE_PHYSICAL: result[fixture_id] = [0] * 40
        result[WASH_PHYSICAL] = [0] * 40
        result[FOCUS_A_PHYSICAL] = _focus_fixed("PK", 0, 0, 0)
        result[FOCUS_B_PHYSICAL] = _focus_fixed("PK", 1, 0, 0)
    elif scene_id == 1:
        for fixture_id in TUBE_PHYSICAL: result[fixture_id] = _solid_tube((0,0,0,140,0))
        result[WASH_PHYSICAL] = _wash_fixed([(0,0,0,0,140,0)] * 6)
        result[FOCUS_A_PHYSICAL] = _focus_fixed("CI", 0, 0, 100)
        result[FOCUS_B_PHYSICAL] = _focus_fixed("CI", 1, 0, 100)
    elif scene_id == 2:
        pixels = [(20,0,55,0,0),(45,0,35,0,0)] * 4
        for fixture_id in TUBE_PHYSICAL: result[fixture_id] = _tube_frame(pixels)
        result[WASH_PHYSICAL] = _wash_fixed([(0,0,20,0,0,110)] * 6)
        result[FOCUS_A_PHYSICAL] = _focus_fixed("PK", 0, 82, 55)
        result[FOCUS_B_PHYSICAL] = _focus_fixed("PK", 1, 37, 55)
    elif scene_id == 3:
        for fixture_id in TUBE_PHYSICAL: result[fixture_id] = _solid_tube((0,0,0,90,45))
        result[WASH_PHYSICAL] = _wash_fixed([(0,0,0,45,90,0)] * 6)
        result[FOCUS_A_PHYSICAL] = _focus_fixed("AN", 0, 0, 70)
        result[FOCUS_B_PHYSICAL] = _focus_fixed("AN", 1, 0, 70)
    elif scene_id == 4:
        palette = ((120,0,0,0,0),(120,80,0,0,25),(95,110,0,0,20),(0,120,0,0,0),
                   (0,100,120,0,0),(0,0,120,0,0),(70,0,120,0,0),(120,0,60,10,0))
        for fixture_id in TUBE_PHYSICAL: result[fixture_id] = _tube_frame(palette)
        result[WASH_PHYSICAL] = _wash_fixed(((120,0,0,0,0,0),(120,100,0,30,0,0),
            (0,120,0,0,0,0),(0,100,120,0,0,0),(0,0,120,0,0,0),(120,0,60,0,10,0)))
        result[FOCUS_A_PHYSICAL] = _focus_fixed("CI", 0, 82, 100)
        result[FOCUS_B_PHYSICAL] = _focus_fixed("CI", 1, 97, 100)
    else:
        raise ValueError(f"Unsupported performance Scene {scene_id}.")
    return result


OVERRIDE_WASH: dict[int, tuple[tuple[int, ...], ...]] = {
    37:((96,0,0,0,0,0),)*6, 38:((96,18,0,70,0,0),)*6,
    39:((96,82,0,25,0,0),)*6, 40:((0,96,0,0,0,0),)*6,
    41:((0,96,96,0,0,0),)*6, 42:((0,0,96,0,0,0),)*6,
    43:((48,0,96,0,0,0),)*6, 44:((96,0,42,0,12,0),)*6,
    45:((96,0,0,0,0,0),(96,82,0,25,0,0),(0,96,0,0,0,0),
        (0,96,96,0,0,0),(0,0,96,0,0,0),(96,0,42,0,12,0)),
}
OVERRIDE_FOCUS = {37:(22,22),38:(67,124),39:(67,67),40:(52,52),41:(97,112),
                  42:(37,37),43:(82,37),44:(82,82),45:(22,37)}


def _override(context: Mapping[str, Any]) -> dict[int, Any]:
    scene_id = int(context["scene_id"]); zones = OVERRIDE_WASH[scene_id]
    expanded = (zones[0],)*8 if scene_id != 45 else (zones[0],zones[1],zones[2],zones[3],zones[4],zones[5],zones[0],zones[5])
    pixels = tuple((c[0],c[1],c[2],c[4],c[3]) for c in expanded)
    result: dict[int, Any] = {fixture_id:_tube_frame(pixels) for fixture_id in TUBE_PHYSICAL}
    wash: dict[int,int] = {}
    for zone, colour in enumerate(zones):
        for component, value in enumerate(colour): wash[4 + zone * 6 + component] = value
    result[WASH_PHYSICAL] = wash
    result[FOCUS_A_PHYSICAL] = {4:OVERRIDE_FOCUS[scene_id][0]}
    result[FOCUS_B_PHYSICAL] = {4:OVERRIDE_FOCUS[scene_id][1]}
    return result


def scene_fixture_values(context: Mapping[str, Any]) -> dict[int, Any]:
    """Return reviewed fixture frames for one live Scene leaf."""
    category = str(context["category"])
    if category == "performance": return _performance(context)
    if category == "override": return _override(context)
    if category == "priority":
        return {WASH_PRIVATE:_wash_from_source(context),
                FOCUS_A_PRIVATE:_focus_from_source(context,0),
                FOCUS_B_PRIVATE:_focus_from_source(context,1)}
    if category == "raw":
        return {WASH_PHYSICAL:_wash_from_source(context),
                FOCUS_A_PHYSICAL:_focus_from_source(context,0),
                FOCUS_B_PHYSICAL:_focus_from_source(context,1)}
    raise ValueError(f"Unsupported creative category {category}.")


def _position_fixture_value(q: Any, fixture_id: int, pan16: int, tilt16: int) -> ET.Element:
    pan, pan_fine = _split16(pan16); tilt, tilt_fine = _split16(tilt16)
    node = ET.Element(q("FixtureVal"), {"ID":str(fixture_id)})
    node.text = f"0,{pan},1,{pan_fine},2,{tilt},3,{tilt_fine}"
    return node


def _add_position_functions(engine: ET.Element, q: Any) -> None:
    existing = {int(node.get("ID","-1")) for node in engine.findall(q("Function"))}
    if existing & set(range(2175,2185)): raise ValueError("Reserved movement Function ID already exists.")
    monitor = engine.find(q("Monitor")); insert_at = list(engine).index(monitor) if monitor is not None else len(engine)
    for function_id, label, preset_name in POSITION_SCENES:
        preset = SOURCE_POSITION_PRESETS[preset_name]
        function = ET.Element(q("Function"), {"ID":str(function_id),"Type":"Scene","Name":label})
        ET.SubElement(function,q("Speed"),{"FadeIn":"0","FadeOut":"0","Duration":"0"})
        function.append(_position_fixture_value(q,FOCUS_A_PHYSICAL,preset[0],preset[1]))
        function.append(_position_fixture_value(q,FOCUS_B_PHYSICAL,preset[2],preset[3]))
        engine.insert(insert_at,function); insert_at += 1
    chaser = ET.Element(q("Function"),{"ID":"2184","Type":"Chaser","Name":"MOVEMENT — FOCUS A/B SWEEP"})
    ET.SubElement(chaser,q("Tempo")).text = "Beats"
    ET.SubElement(chaser,q("Speed"),{"FadeIn":"750","FadeOut":"0","Duration":"1000"})
    ET.SubElement(chaser,q("Direction")).text = "Forward"; ET.SubElement(chaser,q("RunOrder")).text = "Loop"
    ET.SubElement(chaser,q("SpeedModes"),{"FadeIn":"PerStep","FadeOut":"PerStep","Duration":"PerStep"})
    for number, (function_id, _label, _preset) in enumerate(POSITION_SCENES):
        step = ET.SubElement(chaser,q("Step"),{"Number":str(number),"FadeIn":"750","Hold":"250","FadeOut":"0"})
        step.text = str(function_id)
    engine.insert(insert_at,chaser)


def _window(q: Any, parent: ET.Element, x: int, y: int, width: int, height: int) -> None:
    ET.SubElement(parent,q("WindowState"),{"Visible":"True","X":str(x),"Y":str(y),"Width":str(width),"Height":str(height)})


def _appearance(q: Any, parent: ET.Element, foreground: str, background: str, size: int = 14) -> None:
    appearance = ET.SubElement(parent,q("Appearance"))
    ET.SubElement(appearance,q("FrameStyle")).text = "None"
    ET.SubElement(appearance,q("ForegroundColor")).text = foreground
    ET.SubElement(appearance,q("BackgroundColor")).text = background
    ET.SubElement(appearance,q("BackgroundImage")).text = "None"
    ET.SubElement(appearance,q("Font")).text = f"Roboto Condensed,{size},-1,5,700,0,0,0,0,0,0,0,0,0,0,1"


def _widget(virtual_console: ET.Element, q: Any, widget_id: int) -> ET.Element:
    for node in virtual_console.iter():
        if node.get("ID") == str(widget_id) and node.find(q("WindowState")) is not None: return node
    raise ValueError(f"Virtual Console widget {widget_id} is missing.")


def _extend_virtual_console(vc: ET.Element, q: Any, source_ids: Sequence[int]) -> None:
    move_function = _widget(vc,q,1004).find(q("Function"))
    if move_function is None: raise ValueError("MOVE widget has no Function binding.")
    move_function.set("ID","2184")
    _widget(vc,q,1000).set("Caption","CONTROL ONE • FULL-RIG LIVE PERFORMANCE")
    _widget(vc,q,1339).set("Caption","AUTOLOOP INT → global\nGROUP 1 → IR-4 • GROUP 2 → Wash\nGROUP 3 → tubes • GROUP 4 → Focus A/B")
    _widget(vc,q,1343).set("Caption","COLOR PADS → full-rig color override\nSHIFT + WHITE / BLACK / UV → latched effects\nSHIFT + COLOR PADS → Focus A/B positions 1–9")
    next_id = max(map(int,source_ids)) + 1
    page = ET.Element(q("Frame"),{"Caption":"Page 4 — FULL RIG + POSITION BENCH","ID":str(next_id)}); next_id += 1
    _appearance(q,page,"4294507260","4278915616",16); _window(q,page,0,0,1450,760)
    for tag, text in (("AllowResize","False"),("ShowHeader","False"),("ShowEnableButton","True"),("Collapsed","False"),("Disabled","False")):
        ET.SubElement(page,q(tag)).text = text
    enable = ET.SubElement(page,q("EnableSource")); ET.SubElement(enable,q("Key")).text = "Ctrl+Shift+4"
    header = ET.SubElement(page,q("Label"),{"Caption":"FULL RIG • FOCUS A/B POSITION BENCH","ID":str(next_id)}); next_id += 1
    _window(q,header,20,12,1410,48); _appearance(q,header,"4294507260","4279311406",19)
    warning = ET.SubElement(page,q("Label"),{"Caption":"EXACT SHARED SOUND SWITCH VALUES • A/B IS SOURCE ORDER, NOT PROVEN LEFT/RIGHT\nBENCH AIM, INVERSION, FOCUS, AND WASH-ZONE ORDER • REAL FOCUS UV REMAINS DISABLED","ID":str(next_id)}); next_id += 1
    _window(q,warning,20,72,1410,72); _appearance(q,warning,"4294967295","4289331456",15)
    solo = ET.SubElement(page,q("SoloFrame"),{"Caption":"FOCUS A/B POSITIONS • ONE ACTIVE","ID":str(next_id)}); next_id += 1
    _window(q,solo,20,160,1040,520)
    for tag, text in (("AllowResize","False"),("ShowHeader","True"),("ShowEnableButton","True"),("Collapsed","False"),("Disabled","False")):
        ET.SubElement(solo,q(tag)).text = text
    _appearance(q,solo,"4294507260","4279245354",15)
    for index, (function_id, label, _preset) in enumerate(POSITION_SCENES):
        row, column = divmod(index,3)
        button = ET.SubElement(solo,q("Button"),{"Caption":f"{index+1} {label}","ID":str(next_id)}); next_id += 1
        ET.SubElement(button,q("Function"),{"ID":str(function_id)}); ET.SubElement(button,q("Action")).text = "Toggle"
        ET.SubElement(button,q("Input"),{"Universe":"1","Channel":str(164+index)})
        ET.SubElement(button,q("Intensity"),{"Adjust":"False"}).text = "100"
        _window(q,button,24+column*330,48+row*145,300,112); _appearance(q,button,"4294967295","4280833446",14)
    auto = ET.SubElement(page,q("Button"),{"Caption":"MOVE AUTO • 9 SOURCE PRESETS","ID":str(next_id)}); next_id += 1
    ET.SubElement(auto,q("Function"),{"ID":"2184"}); ET.SubElement(auto,q("Action")).text = "Toggle"
    ET.SubElement(auto,q("Intensity"),{"Adjust":"False"}).text = "100"
    _window(q,auto,1100,180,300,120); _appearance(q,auto,"4294967295","4280627783",16)
    patch = ET.SubElement(page,q("Label"),{"Caption":"PATCH\nWash U1 041–080 • 40ch\nFocus A U1 081–098 • 18ch\nFocus B U1 099–116 • 18ch\n\nREAL FOCUS UV DISABLED IN V27","ID":str(next_id)})
    _window(q,patch,1100,330,300,300); _appearance(q,patch,"4294309365","4281163595",15)
    vc.append(page)


def _update_io_labels(engine: ET.Element, q: Any) -> None:
    io_map = engine.find(q("InputOutputMap"))
    if io_map is None: raise ValueError("Input/Output map is missing.")
    universes = {int(node.get("ID","-1")):node for node in io_map.findall(q("Universe"))}
    universes[0].set("Name","Universe 1 — FULL RIG — IR4 + WASH + FOCUS A/B + TUBES")
    universes[2].set("Name","Universe 3 — INTERNAL FULL-RIG PRIORITY — WASH + FOCUS A/B")
    for output in universes[0].findall(q("Output")): output.set("Name",f"{output.get('Name','SoundSwitch')} — FULL RIG 334ch")
    for output in universes[2].findall(q("Output")): output.set("Name","SoundSwitch Hardware — Full-Rig Priority Layer")


def _update_monitor(engine: ET.Element, q: Any) -> None:
    monitor = engine.find(q("Monitor"))
    if monitor is None: raise ValueError("Monitor is missing.")
    grid = monitor.find(q("Grid"))
    if grid is None: raise ValueError("Monitor Grid is missing.")
    grid.set("Width","9"); grid.set("Height","3"); grid.set("Depth","7")
    for node in list(monitor.findall(q("FxItem"))): monitor.remove(node)
    positions = {0:(-4,0,1),1:(-2,0,1),2:(2,0,1),3:(4,0,1),4:(0,0,0),
                 5:(-3,0,-2),6:(-1,0,-2),7:(1,0,-2),8:(3,0,-2),9:(-3,0,3),10:(3,0,3)}
    for fixture_id, (x,y,z) in positions.items():
        ET.SubElement(monitor,q("FxItem"),{"ID":str(fixture_id),"XPos":str(x),"YPos":str(y),"ZPos":str(z)})


def extend_workspace(context: Mapping[str, Any]) -> None:
    if set(context["reserved_function_ids"]) != set(range(2175,2185)):
        raise ValueError("Unexpected reserved movement Function IDs.")
    _add_position_functions(context["engine"],context["q"])
    _extend_virtual_console(context["virtual_console"],context["q"],context["source_widget_ids"])
    _update_io_labels(context["engine"],context["q"])
    _update_monitor(context["engine"],context["q"])


if set(LOOP_PROGRAM) != set(range(532,660)):
    raise RuntimeError("Raw loop program coverage must be exactly 532–659.")
if not {recipe[0] for recipe in LOOP_PROGRAM.values()} <= set(MOVEMENT_SEQUENCES):
    raise RuntimeError("A raw loop references an undefined movement sequence.")
if set(PRIORITY_PROGRAM) != {12,20,21,29,30,32,33,34,35,36}:
    raise RuntimeError("Moving Priority recipe coverage is incomplete.")
