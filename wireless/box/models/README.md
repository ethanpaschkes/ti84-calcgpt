# Ready-made CAD models used by calcbox.scad (and for Ethan's own CAD)

| File | Part | Source, licence |
|---|---|---|
| adafruit_4654_miniboost.step / .stl | Adafruit MiniBoost 5V TPS61023, 17.78 x 11.43 x 5.07 mm | github.com/adafruit/Adafruit_CAD_Parts (MIT) |
| adafruit_1904_microlipo.step / .stl | Adafruit Micro-Lipo MCP73831 charger, 21.0 x 19.05 x 6.37 mm | same |
| adafruit_1317_lipo150.step | Adafruit 150 mAh LiPo (a stand-in for the RC pack) | same |
| kicad_ESP32-S3-WROOM-1.step | the radio module on the DevKitC-1 | KiCad packages3D (CC-BY-SA 4.0 with exception) |
| kicad_TO-92_flat.step | 2N7000 MOSFET lying flat | KiCad packages3D |
| kicad_DO-41_diode.step | 1N5819 diode | KiCad packages3D |
| kicad_resistor_6.3mm.step | 1/4 W resistor | KiCad packages3D |

The DevKitC-1 board itself is drawn in calcbox.scad from Espressif's DXF (no STEP exists from Espressif); the Dupont housings, the
USB-A plug, the OTG adapter and the wires are drawn from measurements. OpenSCAD imports STL only, so the .step files are for a
CAD program (Fusion, FreeCAD, Onshape); calcbox.scad uses the two Adafruit .stl files.

TI-84 Plus CE: no CAD model exists publicly. Closest: TheLastMillennial's 3D scans on Cemetech ("3D Modeling the TI-84 Plus
Family", 2022-2025): a slide-case scan and a 155 MB front-plate scan (.3mf/.stl/.obj/.ply, OneDrive). Community prints are
cases AROUND the calculator (Printables 61133, Cults3D), not the shell. iFixit's teardown shows the shell is held by 6 T6 Torx
screws plus Phillips #00 under the battery, with the board heat-staked to the front shell.
