#!/usr/bin/env python3
from __future__ import annotations
import importlib.util
import pathlib
import struct
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def load(name: str, path: pathlib.Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module

wlanim = load("wlanim", ROOT / "tools" / "wlanim.py")
manifest = load("bret_manifest", ROOT / "tools" / "bret_manifest.py")
wimp = load("wimpimg", ROOT / "tools" / "wimpimg.py")
bundle = load("bret_bundle", ROOT / "tools" / "bret_bundle.py")


def test_wlanim() -> None:
    p = ROOT / "tests" / "fixtures" / "HRTSEQ1_MIN.ASM"
    stand2 = wlanim.extract(p, "hrt_stand2_anim")
    stand4 = wlanim.extract(p, "hrt_stand4_anim")
    torso2 = wlanim.extract(p, "hrt_torso2_anim")
    torso4 = wlanim.extract(p, "hrt_torso4_anim")
    walk2 = wlanim.extract(p, "hrt_walk2_f2_anim")
    walk8 = wlanim.extract(p, "hrt_walk8_f2_anim")
    walk4 = wlanim.extract(p, "hrt_walk4_f4_anim")
    walk6 = wlanim.extract(p, "hrt_walk6_f4_anim")
    run = wlanim.extract_visual_slice(p, "hrt_run_anim", True)

    assert stand2.repeat and len(stand2.frames) == 14
    assert stand2.frames[0].name == "H2ST2A05" and stand2.frames[3].ticks == 6
    assert stand4.repeat and len(stand4.frames) == 14
    assert stand4.frames[0].name == "H4ST4A02" and stand4.frames[6].ticks == 6
    assert torso2.repeat and [f.name for f in torso2.frames] == [
        "H2TW2A01", "H2TW2A02", "H2TW2A03", "H2TW2A04", "H2TW2A03", "H2TW2A02"
    ]
    assert torso4.repeat and torso4.frames[0].name == "H4TW4A01" and len(torso4.frames) == 6
    assert walk2.repeat and len(walk2.frames) == 16
    assert [f.ticks for f in walk2.frames[0:4]] == [2, 2, 2, 3]
    assert walk8.frames[0].name == "H2WL8A16" and walk8.frames[-1].name == "H2WL8A01"
    assert walk4.frames[-1].name == "H4WL4A16"
    assert len(walk6.frames) == 15 and walk6.frames[0].name == "H4WL2A15"
    assert run.repeat and len(run.frames) == 12
    assert run.frames[0].name == "H3RN3A01" and run.frames[-1].name == "H3RN3A12"

    p2 = ROOT / "tests" / "fixtures" / "HRTSEQ2_MIN.ASM"
    punch2 = wlanim.extract_visual_slice(p2, "hrt_2_punch_anim", False)
    punch4 = wlanim.extract_visual_slice(p2, "hrt_4_punch_anim", False)
    kick2 = wlanim.extract_visual_slice(p2, "hrt_2_kick_anim", False)
    kick4 = wlanim.extract_visual_slice(p2, "hrt_4_kick_anim", False)
    assert len(punch2.frames) == 11 and punch2.frames[5].name == "H2PL3B04"
    assert len(punch4.frames) == 11 and punch4.frames[-1].name == "H4PL3X08"
    assert len(kick2.frames) == 12 and kick2.frames[-1].name == "H2KM3A11"
    assert len(kick4.frames) == 12 and kick4.frames[-1].name == "H4KM3B10"


def test_manifest() -> None:
    m = manifest.parse_lod(ROOT / "tests" / "fixtures" / "BRET_MIN.LOD")
    assert m["H4ST4A01"] == "hrt_wlk.img"
    assert m["H2WL1A16"] == "hrt_wlk.img"


def synthetic_wimp(image_name: str = "H4ST4A01", palette_name: str = "HARTPAL") -> bytes:
    # 1 image + 1 palette. Image is 3x2 CI8, padded to a 4-byte row stride.
    # Palette payload at 0x1C; image payload at 0x40; directories start at 0x80.
    assert len(image_name) <= 8 and len(palette_name) <= 8
    data = bytearray(0x100)
    struct.pack_into("<HHIHHHH", data, 0,
                     1, 0, 0x80, 0x063F, 0, 0, 0)
    # transparent black, red, green, blue in Midway RGB555
    struct.pack_into("<HHHH", data, 0x1C, 0x0000, 0x7C00, 0x03E0, 0x001F)
    data[0x40:0x48] = bytes([1, 2, 3, 0xEE, 3, 2, 1, 0xEE])

    off = 0x80
    raw_name = image_name.encode("ascii")
    data[off:off+len(raw_name)] = raw_name
    struct.pack_into("<hhHHHI", data, off + 18,
                     -1, 2, 3, 2, 5, 0x40)
    # 18-byte WIMP tail.  First words are deliberately nonsense to guard
    # against the r6h3 assumption that +32/+34 are runtime attachment coords.
    struct.pack_into("<9h", data, off + 32, 30000, 25000, 0, 7, 11, 2, -1, -1, -1)

    poff = off + wimp.IMAGE_ENTRY_SIZE
    raw_pal = palette_name.encode("ascii")
    data[poff:poff+len(raw_pal)] = raw_pal
    struct.pack_into("<HI", data, poff + 12, 4, 0x1C)
    return bytes(data)


def test_wimp_probe() -> None:
    data = synthetic_wimp()
    h = wimp.parse_header(data)
    entries = wimp.parse_images(data, h)
    pals = wimp.parse_palettes(data, h, entries)
    assert h.image_count == 1 and h.directory_offset == 0x80
    assert entries[0].name == "H4ST4A01"
    assert entries[0].width == 3 and entries[0].height == 2
    assert entries[0].xani == -1 and entries[0].yani == 2
    assert entries[0].tail_words == (30000, 25000, 0, 7, 11, 2, -1, -1, -1)
    assert pals[0].name == "HARTPAL" and pals[0].color_count == 4
    assert wimp.read_palette_words(data, pals[0]) == [0, 0x7C00, 0x03E0, 0x001F]
    assert wimp.read_ci8(data, entries[0]) == bytes([1, 2, 3, 3, 2, 1])
    assert wimp.palette_for_image(entries[0], entries, pals) == pals[0]
    assert wimp.rgb555_to_rgba5551(0x7C00, 1) == 0xF801
    assert wimp.rgb555_to_rgba5551(0x0000, 0) == 0


def test_wimp_emit_c() -> None:
    data = synthetic_wimp()
    h = wimp.parse_header(data)
    entries = wimp.parse_images(data, h)
    pals = wimp.parse_palettes(data, h, entries)
    with tempfile.TemporaryDirectory() as td:
        out = pathlib.Path(td) / "bret_sprites.c"
        wimp.emit_c(out, data, entries, pals, ["H4ST4A01"], "hrt_wlk.img")
        text = out.read_text()
        assert '"H4ST4A01", "hrt_wlk.img", 3, 2, -1, 2, {30000, 25000, 0, 7, 11, 2, -1, -1, -1}' in text
        assert "0xF801" in text
        assert "__attribute__((aligned(8)))" in text
        assert "0x01, 0x02, 0x03, 0x03, 0x02, 0x01" in text


def test_bundle_multi_container() -> None:
    with tempfile.TemporaryDirectory() as td_s:
        td = pathlib.Path(td_s)
        img_dir = td / "IMG"
        img_dir.mkdir()
        (img_dir / "HRT_WLK.IMG").write_bytes(synthetic_wimp("H4ST4A01", "WLKPAL"))
        (img_dir / "HRT_PNC.IMG").write_bytes(synthetic_wimp("H4PL3X01", "PNCPAL"))
        lod = img_dir / "BRET.LOD"
        lod.write_text("hrt_wlk.img\n---> H4ST4A01\nhrt_pnc.img\n---> H4PL3X01\n")
        visual = td / "visual.c"
        visual.write_text('static x a[]={{"H4ST4A01",4},{"H4PL3X01",1}};\n')
        out = td / "bundle.c"
        count, pixels = bundle.emit(out, lod, img_dir, [visual])
        text = out.read_text()
        assert count == 2 and pixels == 12
        assert '"H4ST4A01", "hrt_wlk.img"' in text
        assert '"H4PL3X01", "hrt_pnc.img"' in text
        assert text.count("static uint16_t pal_") == 2


def main() -> int:
    test_wlanim()
    test_manifest()
    test_wimp_probe()
    test_wimp_emit_c()
    test_bundle_multi_container()
    print("source tool tests passed")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
