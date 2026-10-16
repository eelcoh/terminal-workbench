"""Validate exported shaping evidence without freezing host-specific font results."""
import json
from pathlib import Path
import struct
import sys

root=Path(sys.argv[1])
report=json.loads((root/"report.json").read_text())
assert report["platform"] and report["qt"] and report["base_font"]
assert report["font_pixel_size"]==18 and report["cell_width"]>0 and report["cell_height"]>0
samples={sample["id"]:sample for sample in report["samples"]}
assert set(samples)=={"ascii","combining","arabic","indic","cjk","emoji","box","long-cluster"}
for sample in samples.values():
    text=sample["text"];units=len(text.encode("utf-16-le"))//2
    assert "".join(cell["text"] for cell in sample["cells"])==text
    columns=[cell["column"] for cell in sample["cells"]]
    assert columns==sorted(set(columns)) and all(0<=column<80 for column in columns)
    offset=0
    for cell in sample["cells"]:
        assert cell["utf16"]==offset
        offset+=len(cell["text"].encode("utf-16-le"))//2
    for name in ("reference_glyphs","isolated_glyphs"):
        glyphs=sample[name];assert glyphs
        assert all(glyph["font"] and 0<=glyph["utf16"]<units and glyph["glyph"]>=0 for glyph in glyphs)
        missing=sum(glyph["glyph"]==0 for glyph in glyphs)
        assert sample[name.replace("glyphs","missing_glyphs")]==missing
    assert set(sample["reference_fonts"])=={glyph["font"] for glyph in sample["reference_glyphs"]}
assert samples["ascii"]["reference_missing_glyphs"]==0
assert len(samples["long-cluster"]["cells"])==1
assert len(samples["long-cluster"]["cells"][0]["text"])>16
assert [capture["scale"] for capture in report["captures"]]==[1,1.25,1.5,2]
for capture in report["captures"]:
    header=(root/capture["file"]).read_bytes()[:24]
    assert header[:8]==b"\x89PNG\r\n\x1a\n"
    width,height=struct.unpack("!II",header[16:24])
    assert (width,height)==(capture["width"],capture["height"])
    assert (width,height)==(round(1400*capture["scale"]),round(940*capture["scale"]))
print("PASS: complete text/font/glyph reports, long grapheme retention and scaled PNG artifacts")
for sample in samples.values():
    print(sample["id"]+": glyph difference="+str(sample["glyph_identity_difference"])+
          "; missing="+str(sample["reference_missing_glyphs"])+"; fonts="+", ".join(sample["reference_fonts"]))
