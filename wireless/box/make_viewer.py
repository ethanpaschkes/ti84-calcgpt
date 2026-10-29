"""Builds calc-box-3d.html: a rotatable viewer with three scenes (dupont box, soldered box, the pack on the calculator)."""
import base64, json, os
here = os.path.dirname(os.path.abspath(__file__))
def b64(name): return base64.b64encode(open(os.path.join(here, name), "rb").read()).decode()

scenes = {
    "dupont": {"title": "Box, Dupont wiring (as you have it)", "dims": {"w": 30.6, "h": 28.16}, "target": [30, 15, 10], "cam": [150, -190, 120],
               "meshes": {k: ("%s_%s.stl" % ("dupont", k)) for k in ("base", "lid", "devkit", "parts", "wires", "cable")}},
    "soldered": {"title": "Box, soldered (slim)", "dims": {"w": 30.6, "h": 16.53}, "target": [30, 15, 8], "cam": [150, -190, 120],
                 "meshes": {k: ("%s_%s.stl" % ("soldered", k)) for k in ("base", "lid", "devkit", "parts", "wires", "cable")}},
    "oncalc": {"title": "On the calculator", "dims": None, "target": [43, 96, -10], "cam": [-260, -220, -230],
               "meshes": {"calc": "scene_calc_nocover.stl", "cover": "scene_cover_only.stl", "pack": "scene_pack_only.stl"}},
}
colors = {"base": ["0x2f8ea3", 0.5], "lid": ["0x2f8ea3", 0.5], "devkit": ["0x2a3440", 1], "parts": ["0x9aa7b0", 1], "wires": ["0xd0342c", 1],
          "cable": ["0x444444", 1], "calc": ["0x23262c", 1], "cover": ["0xf2f1ec", 1], "pack": ["0x2f8ea3", 1]}
data = {name: {"title": s["title"], "dims": s["dims"], "target": s["target"], "cam": s["cam"], "meshes": {k: b64(f) for k, f in s["meshes"].items()}} for name, s in scenes.items()}

html = r'''<title>Calc Box 1:1</title>
<meta name="claude:viewport-fit-sides" content="cover">
<style>
:root { --bg:#ecebe6; --fg:#1e1e1b; --muted:#66655e; --panel:#dcdad2; --accent:#1f6f8b; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg:#15161a; --fg:#e8e7e1; --muted:#a3a29a; --panel:#22242a; --accent:#5fb6d6; color-scheme: dark } }
:root[data-theme="dark"] { --bg:#15161a; --fg:#e8e7e1; --muted:#a3a29a; --panel:#22242a; --accent:#5fb6d6; color-scheme: dark }
body { background: var(--bg); color: var(--fg); font-family: system-ui, sans-serif; padding-block: 16px; padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
main { max-width: 1000px; margin: 0 auto; display: grid; gap: 12px; }
h1 { font-size: 20px; font-weight: 600; margin: 0; }
#view { width: 100%; aspect-ratio: 16 / 10; max-width: 100%; background: var(--panel); border-radius: 12px; touch-action: none; }
.row { display: flex; flex-wrap: wrap; gap: 8px 14px; align-items: center; font-size: 14px; color: var(--muted); }
button { font: inherit; padding: 6px 12px; border-radius: 8px; border: 1px solid var(--muted); background: transparent; color: var(--fg); cursor: pointer; }
button[aria-pressed="true"] { border-color: var(--accent); color: var(--accent); }
label { display: inline-flex; gap: 6px; align-items: center; }
table { border-collapse: collapse; font-size: 13px; width: 100%; } td, th { text-align: left; padding: 4px 8px; border-bottom: 1px solid var(--panel); vertical-align: top; } td:nth-child(2) { font-variant-numeric: tabular-nums; }
</style>
<main>
<h1>Calc box, 1:1 model</h1>
<div class="row" id="scenes"></div>
<div class="row" id="toggles"></div>
<canvas id="view"></canvas>
<p class="row">Drag to turn, scroll to zoom, right-drag to move. Millimetres. In the calculator view the pack's cable end sits 22 mm below the top edge, where the mini-USB port is.</p>
<table>
<tr><th>Item</th><th>Size (mm)</th><th>Source</th></tr>
<tr><td>Box, Dupont</td><td>85.45 × 30.6 × 28.16 outside</td><td>header 2.5 + housing 14 + bend 3 under the board; 11 mm room for 1.8 m of coiled wire</td></tr>
<tr><td>Box, soldered</td><td>82.45 × 30.6 × 16.53 outside</td><td>pins cut flush; height set by the 6.4 mm charger</td></tr>
<tr><td>TI-84 Plus CE body</td><td>192.8 × 86.9 × 19, cover adds 1.3; screen window 62 × 48</td><td>TI: 7.59 × 3.42 × 0.80 in with cover; layout from your photos (about 1 mm)</td></tr>
<tr><td>ESP32-S3-DevKitC-1 v1.1</td><td>25.4 × 62.8, module to 69.15, USB-C lips to −1.3</td><td>Espressif DXF 2022-04-29</td></tr>
<tr><td>Pins</td><td>2 × 22 at 2.54; header 2.5 + pin 6.0 under the board, or 1 mm stubs</td><td>Espressif DXF; standard header</td></tr>
<tr><td>Dupont housing, jumper wire</td><td>2.54 × 2.54 × 14.0; wire 1.2, 30 cm and 20 cm</td><td>your MEIRIYFA cable and EDGELEC jumpers: measure</td></tr>
<tr><td>USB-A plug + OTG adapter</td><td>14 × 7 × 32 and 16 × 8 × 36, about 80 mm chain</td><td>your MEIRIYFA plug and rgzhihuifz adapter: measure</td></tr>
<tr><td>LiPo</td><td>17 × 30 × 4</td><td>guess for a 150 mAh pouch: measure your RC pack</td></tr>
<tr><td>Charger, Adafruit Micro-Lipo 1904</td><td>19.05 × 21.0 × 6.37</td><td>Adafruit CAD</td></tr>
<tr><td>Boost, Adafruit MiniBoost 4654</td><td>17.78 × 11.43 × 5.07</td><td>Adafruit CAD</td></tr>
<tr><td>2N7000, 1N5819, 2 × 100 k</td><td>4.6 × 5.2 × 3.7, 2.7 × 4.1, 2.4 × 6.3</td><td>package datasheets (KiCad STEP in models/)</td></tr>
</table>
</main>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/build/three.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
<script>
const SCENES = __DATA__;
const COLORS = __COLORS__;
function stl(b64) {
  const bin = atob(b64), n = bin.length, buf = new ArrayBuffer(n), u8 = new Uint8Array(buf);
  for (let i = 0; i < n; i++) u8[i] = bin.charCodeAt(i);
  const dv = new DataView(buf), count = dv.getUint32(80, true), pos = new Float32Array(count * 9), nor = new Float32Array(count * 9);
  for (let t = 0; t < count; t++) { const o = 84 + t * 50, nx = dv.getFloat32(o, true), ny = dv.getFloat32(o + 4, true), nz = dv.getFloat32(o + 8, true);
    for (let v = 0; v < 3; v++) { const p = o + 12 + v * 12, k = t * 9 + v * 3; pos[k] = dv.getFloat32(p, true); pos[k + 1] = dv.getFloat32(p + 4, true); pos[k + 2] = dv.getFloat32(p + 8, true); nor[k] = nx; nor[k + 1] = ny; nor[k + 2] = nz; } }
  const g = new THREE.BufferGeometry(); g.setAttribute("position", new THREE.BufferAttribute(pos, 3)); g.setAttribute("normal", new THREE.BufferAttribute(nor, 3)); return g;
}
const canvas = document.getElementById("view");
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true }); renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(35, 1.6, 1, 3000); camera.up.set(0, 0, 1);
const controls = new THREE.OrbitControls(camera, canvas);
scene.add(new THREE.HemisphereLight(0xffffff, 0x667788, 0.9));
const sun = new THREE.DirectionalLight(0xffffff, 0.7); sun.position.set(80, -120, 160); scene.add(sun);
const sun2 = new THREE.DirectionalLight(0xffffff, 0.4); sun2.position.set(-100, 100, -160); scene.add(sun2);
const mat = (c, o) => new THREE.MeshStandardMaterial({ color: parseInt(c), roughness: 0.65, metalness: 0.05, transparent: o < 1, opacity: o, side: THREE.DoubleSide });
const group = new THREE.Group(); scene.add(group);
const geoms = {};
let cur = null, meshes = {}, lifted = true, boxShown = true;
function build(name) {
  cur = name; const S = SCENES[name];
  while (group.children.length) group.remove(group.children[0]);
  meshes = {};
  for (const [k, b] of Object.entries(S.meshes)) {
    const key = name + "/" + k; if (!geoms[key]) geoms[key] = stl(b);
    const m = new THREE.Mesh(geoms[key], mat(COLORS[k][0], COLORS[k][1])); meshes[k] = m; group.add(m);
    if (k === "lid") { m.rotation.x = Math.PI; m.position.set(0, S.dims.w, S.dims.h); }
  }
  camera.position.set(...S.cam); controls.target.set(...S.target); controls.update();
  for (const b of document.querySelectorAll("#scenes button")) b.setAttribute("aria-pressed", String(b.dataset.scene === name));
  applyToggles();
}
function applyToggles() {
  const S = SCENES[cur];
  if (meshes.lid) meshes.lid.position.z = lifted ? S.dims.h + 26 : S.dims.h;
  if (meshes.base) meshes.base.visible = boxShown; if (meshes.lid) meshes.lid.visible = boxShown;
  for (const k of ["wires", "parts", "cable", "cover"]) if (meshes[k]) meshes[k].visible = document.getElementById("t-" + k).checked;
  document.getElementById("b-lid").textContent = lifted ? "Lid lifted" : "Lid closed";
  const box = cur !== "oncalc"; document.getElementById("b-lid").hidden = !box; document.getElementById("b-box").hidden = !box;
  for (const k of ["wires", "parts", "cable"]) document.getElementById("t-" + k).parentElement.hidden = !box;
  document.getElementById("t-cover").parentElement.hidden = box;
}
const sc = document.getElementById("scenes");
for (const [name, S] of Object.entries(SCENES)) { const b = document.createElement("button"); b.textContent = S.title; b.dataset.scene = name; b.onclick = () => build(name); sc.appendChild(b); }
const tg = document.getElementById("toggles");
tg.innerHTML = '<button id="b-lid">Lid lifted</button> <button id="b-box">Hide the box</button>' +
  ['wires', 'parts', 'cable', 'cover'].map(k => `<label><input type="checkbox" id="t-${k}" checked> ${ {wires: "wires", parts: "LiPo, charger, boost, small parts", cable: "plug + OTG adapter outside", cover: "slide cover"}[k] }</label>`).join(" ");
document.getElementById("b-lid").onclick = () => { lifted = !lifted; applyToggles(); };
document.getElementById("b-box").onclick = e => { boxShown = !boxShown; e.target.textContent = boxShown ? "Hide the box" : "Show the box"; applyToggles(); };
for (const k of ["wires", "parts", "cable", "cover"]) document.getElementById("t-" + k).onchange = applyToggles;
build("dupont");
function resize() { const w = canvas.clientWidth, h = canvas.clientHeight, r = renderer.getPixelRatio(); if (canvas.width !== w * r || canvas.height !== h * r) { renderer.setSize(w, h, false); camera.aspect = w / h; camera.updateProjectionMatrix(); } }
(function tick() { resize(); controls.update(); renderer.render(scene, camera); requestAnimationFrame(tick); })();
</script>
'''
html = html.replace("__DATA__", json.dumps(data)).replace("__COLORS__", json.dumps(colors))
out = os.path.join(here, "..", "calc-box-3d.html")
open(out, "w").write(html)
open(os.path.join(here, "..", "serve", "index.html"), "w").write(html)
print(len(html) // 1024, "KB")
