"""Builds calc-box-3d.html next to this script: a rotatable three.js viewer of the TI-84 Plus CE with the Calc box on its back.
Scenes: the recommended pack (soldered, flat, side cable exit) on the bare back, the same pack printed onto a slide cover, the
no-soldering Dupont pack, and the two boxes on their own with the lid lifted. It exports the STL files with OpenSCAD first (into
stl/), reads the sizes and fit reports from OpenSCAD's echo output, and embeds everything in one HTML file.
Run: python3 make_viewer.py   (needs only the standard library and OpenSCAD)"""
import base64, json, os, re, shutil, subprocess
from concurrent.futures import ThreadPoolExecutor

here = os.path.dirname(os.path.abspath(__file__))
stl_dir = os.path.join(here, "stl")
OPENSCAD = shutil.which("openscad") or "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"

REC = {"pack_wiring": "soldered", "pack_layout": "flat", "pack_cable_exit": "side"}     # the recommendation in LAYOUTS.md
DUP = {"pack_wiring": "dupont", "pack_layout": "stacked", "pack_cable_exit": "side"}     # no soldering
BOX_REC = {"wiring": "soldered", "layout": "flat", "cable_exit": "side"}
BOX_DUP = {"wiring": "dupont", "layout": "stacked", "cable_exit": "side"}


def defines(d):
    return [a for k, v in d.items() for a in ("-D", "%s=%s" % (k, json.dumps(v)))]


def openscad(out, scad, d, fmt=None):
    cmd = [OPENSCAD] + (["--export-format", fmt] if fmt else []) + ["-o", out] + defines(d) + [os.path.join(here, scad)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    bad = [l for l in r.stderr.splitlines() if "WARNING" in l or "ERROR" in l]
    if r.returncode or bad:
        raise SystemExit("OpenSCAD failed for %s:\n%s" % (out, "\n".join(bad) or r.stderr))


def echo(scad, d):
    out = os.path.join(stl_dir, "report_%d.echo" % abs(hash(json.dumps([scad, d], sort_keys=True))))
    openscad(out, scad, d)
    lines = [l[len('ECHO: "'):-1] for l in open(out).read().splitlines() if l.startswith('ECHO: "')]
    os.remove(out)
    return lines


# every mesh: file name -> (scad file, defines)
calc_scene = lambda name, extra=None: ("ti84ce.scad", dict({"scene": name}, **(extra or {})))
meshes = {
    "calc": calc_scene("calc_nocover"),
    "rec_box": calc_scene("pack_box_only", REC), "rec_inside": calc_scene("pack_inside_only", REC), "rec_cable": calc_scene("pack_cable_only", REC),
    "cov_cover": calc_scene("cover_pocket_only", REC),
    "cov_box": calc_scene("pack_box_only", dict(REC, pack_place="cover")), "cov_inside": calc_scene("pack_inside_only", dict(REC, pack_place="cover")),
    "cov_cable": calc_scene("pack_cable_only", dict(REC, pack_place="cover")),
    "dup_box": calc_scene("pack_box_only", DUP), "dup_inside": calc_scene("pack_inside_only", DUP), "dup_cable": calc_scene("pack_cable_only", DUP),
}
for tag, d in (("boxrec", BOX_REC), ("boxdup", BOX_DUP)):
    for p in ("base", "lid", "devkit", "parts", "wires", "cable"):
        meshes["%s_%s" % (tag, p)] = ("calcbox.scad", dict(d, part=p))

os.makedirs(stl_dir, exist_ok=True)
with ThreadPoolExecutor(max_workers=6) as pool:
    list(pool.map(lambda kv: openscad(os.path.join(stl_dir, kv[0] + ".stl"), kv[1][0], kv[1][1], "binstl"), meshes.items()))


def b64(name):
    return base64.b64encode(open(os.path.join(stl_dir, name + ".stl"), "rb").read()).decode()


def box_dims(d):
    m = re.search(r"box outside ([\d.]+) x ([\d.]+) x ([\d.]+)", echo("calcbox.scad", dict(d, part="none"))[0])
    return {"l": float(m.group(1)), "w": float(m.group(2)), "h": float(m.group(3))}


def placement_report(scene, d):
    lines = echo("ti84ce.scad", dict(d, scene=scene))
    return [l.strip() for l in lines[next(i for i, l in enumerate(lines) if l.startswith("PLACEMENT")):]]


calc_view = {"target": [45, 150, -6], "cam": [300, 20, -260]}
box_view = {"target": [40, 25, 6], "cam": [150, -190, 120]}
scenes = {
    "rec": dict(calc_view, title="Recommended: soldered, flat, top of the back", report=placement_report("pack_top", REC),
                meshes={"calc": "calc", "box": "rec_box", "inside": "rec_inside", "cable": "rec_cable"}),
    "cover": dict(calc_view, title="Same pack printed onto a slide cover", report=placement_report("pack_cover", REC),
                  meshes={"calc": "calc", "cover": "cov_cover", "box": "cov_box", "inside": "cov_inside", "cable": "cov_cable"}),
    "dupont": dict(calc_view, title="No soldering: Dupont, stacked", report=placement_report("pack_top", DUP),
                   meshes={"calc": "calc", "box": "dup_box", "inside": "dup_inside", "cable": "dup_cable"}),
    "boxrec": dict(box_view, title="Box: soldered, flat (lid lifted)", dims=box_dims(BOX_REC), report=echo("calcbox.scad", dict(BOX_REC, part="none")),
                   meshes={k: "boxrec_" + k for k in ("base", "lid", "devkit", "parts", "wires", "cable")}),
    "boxdup": dict(box_view, title="Box: Dupont, stacked (lid lifted)", dims=box_dims(BOX_DUP), report=echo("calcbox.scad", dict(BOX_DUP, part="none")),
                   meshes={k: "boxdup_" + k for k in ("base", "lid", "devkit", "parts", "wires", "cable")}),
}
colors = {"calc": ["0x23262c", 1], "cover": ["0xf2f1ec", 1], "box": ["0x2f8ea3", 0.55], "inside": ["0x9aa7b0", 1], "cable": ["0x444444", 1],
          "base": ["0x2f8ea3", 0.5], "lid": ["0x2f8ea3", 0.5], "devkit": ["0x2a3440", 1], "parts": ["0x9aa7b0", 1], "wires": ["0xd0342c", 1]}
labels = {"calc": "calculator", "cover": "cover with pocket", "box": "box + lid", "inside": "board, parts, wires", "cable": "cable", "base": "box",
          "lid": "lid", "devkit": "DevKit", "parts": "LiPo, charger, boost, small parts", "wires": "wires", }
for s in scenes.values():
    s["meshes"] = {k: b64(f) for k, f in s["meshes"].items()}

html = r'''<!doctype html>
<meta charset="utf-8">
<title>Calc Box 1:1</title>
<style>
:root { --bg:#ecebe6; --fg:#1e1e1b; --muted:#66655e; --panel:#dcdad2; --accent:#1f6f8b; }
@media (prefers-color-scheme: dark) { :root { --bg:#15161a; --fg:#e8e7e1; --muted:#a3a29a; --panel:#22242a; --accent:#5fb6d6; color-scheme: dark } }
body { background: var(--bg); color: var(--fg); font-family: system-ui, sans-serif; padding: 16px; }
main { max-width: 1000px; margin: 0 auto; display: grid; gap: 12px; }
h1 { font-size: 20px; font-weight: 600; margin: 0; }
#view { width: 100%; aspect-ratio: 16 / 10; background: var(--panel); border-radius: 12px; touch-action: none; }
.row { display: flex; flex-wrap: wrap; gap: 8px 14px; align-items: center; font-size: 14px; color: var(--muted); }
button { font: inherit; padding: 6px 12px; border-radius: 8px; border: 1px solid var(--muted); background: transparent; color: var(--fg); cursor: pointer; }
button[aria-pressed="true"] { border-color: var(--accent); color: var(--accent); }
label { display: inline-flex; gap: 6px; align-items: center; }
#report { font: 13px/1.5 ui-monospace, monospace; white-space: pre-wrap; background: var(--panel); border-radius: 8px; padding: 10px 12px; margin: 0; }
</style>
<main>
<h1>Calc box on the TI-84 Plus CE, 1:1</h1>
<div class="row" id="scenes"></div>
<div class="row" id="toggles"></div>
<canvas id="view"></canvas>
<p class="row">Drag to turn, scroll to zoom, right-drag to move. Millimetres. The calculator is seen from the back; its mini-USB port is on the right edge. Numbers below come straight from the OpenSCAD fit report (see LAYOUTS.md).</p>
<pre id="report"></pre>
</main>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/build/three.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
<script>
const SCENES = __DATA__;
const COLORS = __COLORS__;
const LABELS = __LABELS__;
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
const sun2 = new THREE.DirectionalLight(0xffffff, 0.5); sun2.position.set(100, 100, -160); scene.add(sun2);
const mat = (c, o) => new THREE.MeshStandardMaterial({ color: parseInt(c), roughness: 0.65, metalness: 0.05, transparent: o < 1, opacity: o, side: THREE.DoubleSide });
const group = new THREE.Group(); scene.add(group);
const geoms = {};
let cur = null, meshes = {}, lifted = true;
function build(name) {
  cur = name; const S = SCENES[name];
  while (group.children.length) group.remove(group.children[0]);
  meshes = {};
  for (const [k, b] of Object.entries(S.meshes)) {
    const key = name + "/" + k; if (!geoms[key]) geoms[key] = stl(b);
    const m = new THREE.Mesh(geoms[key], mat(COLORS[k][0], COLORS[k][1])); meshes[k] = m; group.add(m);
    if (k === "lid") m.rotation.x = Math.PI;   // the lid is exported upside down (its outer face on the bed); turn it onto the box
  }
  camera.position.set(...S.cam); controls.target.set(...S.target); controls.update();
  for (const b of document.querySelectorAll("#scenes button")) b.setAttribute("aria-pressed", String(b.dataset.scene === name));
  const tg = document.getElementById("toggles");
  tg.innerHTML = (meshes.lid ? '<button id="b-lid"></button> ' : "") + Object.keys(meshes).filter(k => k !== "lid").map(k => `<label><input type="checkbox" data-k="${k}" checked> ${LABELS[k]}</label>`).join(" ");
  if (meshes.lid) document.getElementById("b-lid").onclick = () => { lifted = !lifted; applyToggles(); };
  for (const c of tg.querySelectorAll("input")) c.onchange = applyToggles;
  document.getElementById("report").textContent = S.report.join("\n");
  applyToggles();
}
function applyToggles() {
  const S = SCENES[cur];
  for (const c of document.querySelectorAll("#toggles input")) meshes[c.dataset.k].visible = c.checked;
  if (meshes.lid) {
    meshes.lid.position.set(0, S.dims.w, S.dims.h + (lifted ? 26 : 0));
    if (meshes.base) meshes.lid.visible = meshes.base.visible;
    document.getElementById("b-lid").textContent = lifted ? "Lid lifted" : "Lid closed";
  }
}
const sc = document.getElementById("scenes");
for (const [name, S] of Object.entries(SCENES)) { const b = document.createElement("button"); b.textContent = S.title; b.dataset.scene = name; b.onclick = () => build(name); sc.appendChild(b); }
build("rec");
function resize() { const w = canvas.clientWidth, h = canvas.clientHeight, r = renderer.getPixelRatio(); if (canvas.width !== w * r || canvas.height !== h * r) { renderer.setSize(w, h, false); camera.aspect = w / h; camera.updateProjectionMatrix(); } }
(function tick() { resize(); controls.update(); renderer.render(scene, camera); requestAnimationFrame(tick); })();
</script>
'''
html = html.replace("__DATA__", json.dumps(scenes)).replace("__COLORS__", json.dumps(colors)).replace("__LABELS__", json.dumps(labels))
out = os.path.join(here, "calc-box-3d.html")
open(out, "w").write(html)
print("%s: %.1f MB, %d scenes, %d meshes" % (out, len(html) / 1e6, len(scenes), len(meshes)))
