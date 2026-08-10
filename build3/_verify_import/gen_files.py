import zipfile
import os

os.makedirs(os.path.dirname(os.path.abspath(__file__)), exist_ok=True)
base = os.path.dirname(os.path.abspath(__file__))

ggb_xml = """<?xml version="1.0" encoding="utf-8"?>
<geogebra format="5.0" app="geometry">
  <construction>
    <element type="point" label="A">
      <coords x="0" y="0" z="1"/>
    </element>
    <element type="point" label="B">
      <coords x="4" y="0" z="1"/>
    </element>
    <element type="point" label="C">
      <coords x="0" y="3" z="1"/>
    </element>
    <element type="segment" label="a">
      <startPoint x="0" y="0"/>
      <endPoint x="4" y="0"/>
    </element>
    <element type="line" label="f">
      <startPoint x="0" y="0"/>
      <endPoint x="1" y="1"/>
    </element>
    <element type="circle" label="c">
      <equation>((x - (0))^(2)) + ((y - (0))^(2)) = (4)</equation>
      <center x="0" y="0"/>
    </element>
    <element type="polygon" label="poly1">
      <points>
        <point x="0" y="0"/>
        <point x="4" y="0"/>
        <point x="0" y="3"/>
      </points>
    </element>
  </construction>
</geogebra>
"""

with zipfile.ZipFile(os.path.join(base, "test.ggb"), "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("geogebra.xml", ggb_xml)

svg = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <rect x="10" y="10" width="20" height="20"/>
  <circle cx="50" cy="50" r="10"/>
  <line x1="0" y1="0" x2="100" y2="100"/>
  <polyline points="0,0 10,0 10,10"/>
  <polygon points="20,20 30,20 25,30"/>
  <path d="M 0 0 L 10 0 L 10 10 Z"/>
</svg>
"""
with open(os.path.join(base, "test.svg"), "w", encoding="utf-8") as f:
    f.write(svg)

print("generated: test.ggb, test.svg")
