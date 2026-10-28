#!/usr/bin/env python3
"""Have the local model write physics simulations for the TI-84 Plus CE, compile each one,
and feed compiler errors back until it builds (up to 5 tries). Needs Ollama + the CEdev toolchain.

It never competes with Blender: it waits while any Blender render or the Apex Sky render script runs,
lets Ollama unload the model while it waits, and unloads it for good when finished.
Results go to RESULTS.md. Programs are only checked by compiling, NOT run on a calculator.
"""
import json, os, re, subprocess, sys, time, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
MODEL = "ti84coder"
MAX_TRIES = 5
ACTIVE_BUDGET = 3 * 3600                    # seconds of actual model work; time spent waiting for renders does not count
spent = 0.0
CEDEV = os.path.expanduser("~/CEdev")
ENV = dict(os.environ, CEDEV=CEDEV, PATH=CEDEV + "/bin:" + os.environ["PATH"])

SKELETON = """#include <graphx.h>
#include <keypadc.h>
#include <sys/timers.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

int main(void)
{
    gfx_Begin();
    gfx_SetDrawBuffer();            /* draw off-screen, then gfx_SwapDraw() */
    for (;;) {
        kb_Scan();
        if (kb_Data[6] & kb_Clear) break;       /* Clear quits */
        if (kb_Data[7] & kb_Left) { /* arrow keys are in kb_Data[7]: kb_Left kb_Right kb_Up kb_Down */ }
        gfx_FillScreen(255);                    /* 255 = white, 0 = black, 0xE0 red, 0x03 green, 0x10 blue */
        gfx_SetColor(0xE0);
        gfx_FillCircle(160, 120, 5);
        gfx_SetTextFGColor(0);
        gfx_PrintStringXY("Hello", 10, 10);
        gfx_SetTextXY(10, 22); gfx_PrintInt(42, 3);   /* integers only: there is no printf for floats */
        gfx_SwapDraw();
        delay(15);
    }
    gfx_End();
    return 0;
}
"""

RULES = """Write ONE complete C program for the TI-84 Plus CE using the CEdev toolchain, as in this skeleton:

{skeleton}
Rules:
- Screen is 320x240. Use graphx.h and keypadc.h only (plus math.h, stdlib.h, stdint.h, sys/timers.h).
- Use float (not double). No malloc, no C++, no printf of floats. Show numbers as integers (multiply by 10 or 100 to show decimals).
- Keep arrays small and static. Keep per-frame work light: the CPU is only 48 MHz with software floating point.
- Controls on screen: draw a short help line at the bottom. Clear must always quit. Call gfx_End() before returning.
- Use these key names only: kb_Left kb_Right kb_Up kb_Down (in kb_Data[7]), kb_Enter kb_Clear kb_Add kb_Sub (in kb_Data[6]), kb_2nd kb_Mode (in kb_Data[1]).
- Edge-detect keys that should act once per press (keep a 'was down' flag).
- Real physics, correct units, and clear labels. Do not invent library functions.
- Output ONLY the C source in a single ```c code block, no explanation.

THE PROGRAM:
{spec}
"""

SPECS = [
 ("GASBOX", "Gas in a box (Boyle, Charles, Gay-Lussac). About 30 point particles move in a rectangular container drawn on screen. The right wall is a movable piston: Left/Right move it, changing the volume. Up/Down raise or lower the temperature by scaling every particle's speed by 1.1 or 1/1.1. Particles bounce elastically off the walls (ignore particle-particle collisions). Measure pressure as the total momentum hitting the walls per second, averaged over about one second, and show it as an integer. Show on screen: volume (piston distance), temperature (proportional to the mean kinetic energy), pressure, and P*V. Show 'P x V stays ~constant if T is constant' hints. Enter resets."),
 ("MAXWELL", "Maxwell-Boltzmann speed distribution. About 50 particles in a box with elastic wall bounces and simple 2D elastic particle-particle collisions (equal masses; check pairs each frame, O(n^2) is fine). Start every particle with the SAME speed in random directions and watch the speed distribution spread out into the bell-like shape. Draw a live histogram of speeds (10 bins) on the right side and show the mean speed as an integer. Up/Down change temperature by scaling all speeds."),
 ("EFFUSE", "Graham's law of effusion. A left chamber holds 20 light red particles (mass 1) and 20 heavy blue particles (mass 4) at the same temperature, so average speed is proportional to 1/sqrt(mass): red particles move twice as fast. A small hole in the wall leads to an empty right chamber. Particles that go through the hole count as escaped (they keep moving in the right chamber). Show the escaped count for each color and the ratio red/blue as a number with one decimal. Enter restarts the experiment."),
 ("DALTON", "Dalton's law of partial pressures. A box with three kinds of gas particles (red, green, blue) bouncing elastically off the walls. Keys 1, 2, 3 are not available, so use Left to add a red particle, Up to add a green particle, Right to add a blue particle (max 20 each), Down to remove a particle of the most numerous color. Measure each gas's pressure as wall-collision momentum per second (averaged over about a second). Draw three bars for the partial pressures and one bar for the total pressure, and show that total = sum of the partials. Clear quits."),
 ("CHARLES", "Charles's law with a heated gas. A cylinder drawn on screen with a piston that is pushed up by 25 particles at constant pressure: the piston has a fixed weight, so it rises until the particles' collisions balance it (let the piston move according to net momentum transfer with a constant downward force). Up/Down change the temperature by scaling particle speeds. Show the volume (piston height) and temperature as integers and the ratio V/T so the student sees it stays roughly constant."),
 ("PROJECT", "Projectile motion. Left/Right change the launch angle (5 to 85 degrees), Up/Down change the launch speed (5 to 40 m/s), Enter fires. Gravity 9.8 m/s^2, no air drag. Draw the ground, the trail of the projectile, and show the time of flight, maximum height and range as integers computed from the physics, plus the live x and y. Scale the world to fit the screen automatically."),
 ("PENDULUM", "Simple pendulum. Left/Right change the pendulum length (0.5 to 2.0 m), Up/Down change the starting angle (5 to 80 degrees), Enter releases it. Integrate the exact equation theta'' = -(g/L) sin(theta) with small time steps (velocity Verlet). Draw the bob and string and a graph of angle versus time. Show the measured period (time between two upward zero crossings) next to the small-angle formula 2*pi*sqrt(L/g), both with two decimals as integers (x100)."),
 ("SPRING", "Mass on a spring with damping. Left/Right change the damping from 0 to a heavy value, Up/Down change the spring constant. Enter pulls the mass down and releases it. Integrate x'' = -(k/m)x - (c/m)v. Draw the spring and mass on the left and a graph of displacement versus time on the right. Show k, damping and whether it is underdamped, critically damped or overdamped (compare c with 2*sqrt(k*m))."),
 ("ORBIT", "Two-body orbit. A planet orbits a fixed sun at the screen center under inverse-square gravity (use units where GM = 1000 and distances in pixels). Up/Down change the planet's launch speed, Enter relaunches from the starting point. Integrate with small steps (velocity Verlet), draw the sun, the planet and its fading trail. Show the speed and the distance from the sun, and say whether the orbit is circular, elliptical or escaping (compare energy to zero)."),
 ("COLLIDE", "One-dimensional collisions. Two carts on a track with adjustable masses (Up/Down for the left cart, Left/Right for the right cart, 1 to 10 kg) and the left cart starts at 5 m/s toward the right cart at rest. Enter runs the collision; Add key (kb_Add) toggles elastic versus perfectly inelastic. Animate the carts, and show total momentum and total kinetic energy before and after as integers, so the student sees momentum is always conserved and kinetic energy only in the elastic case."),
]

def gpu_busy():
    for pat in ("Blender -b", "Blender.app/Contents/MacOS/Blender", "pov-overnight.sh", "render_orbit.py"):
        if subprocess.run(["pgrep", "-f", pat], capture_output=True).returncode == 0:
            return True
    return False

def wait_for_gpu():
    waited = False
    while gpu_busy():
        if not waited:
            log("waiting: a render is running (the model stays unloaded)")
            waited = True
        time.sleep(30)
    if waited:
        log("render finished, continuing")

def log(msg):
    line = time.strftime("%H:%M:%S ") + msg
    print(line, flush=True)
    with open(os.path.join(HERE, "run.log"), "a") as f:
        f.write(line + "\n")

def generate(prompt):
    body = json.dumps({"model": MODEL, "prompt": prompt, "stream": False, "think": False, "keep_alive": "3m",
                       "options": {"temperature": 0.3, "num_predict": 5000, "num_ctx": 12288}}).encode()
    req = urllib.request.Request("http://localhost:11434/api/generate", body, {"Content-Type": "application/json"})
    return json.load(urllib.request.urlopen(req, timeout=1800))["response"]

def extract_c(text):
    m = re.search(r"```c\s*\n(.*?)```", text, re.S) or re.search(r"```\s*\n(.*?)```", text, re.S)
    return (m.group(1) if m else text).strip() + "\n"

def build(name, code):
    d = os.path.join(HERE, name)
    os.makedirs(os.path.join(d, "src"), exist_ok=True)
    mk = open(os.path.join(HERE, "..", "chat", "makefile")).read()
    mk = re.sub(r"(?m)^NAME = .*", "NAME = " + name, mk)
    mk = re.sub(r'(?m)^DESCRIPTION = .*', 'DESCRIPTION = "%s"' % name.title(), mk)
    open(os.path.join(d, "makefile"), "w").write(mk)
    if not os.path.exists(os.path.join(d, "icon.png")):
        open(os.path.join(d, "icon.png"), "wb").write(open(os.path.join(HERE, "icon.png"), "rb").read())
    open(os.path.join(d, "src", "main.c"), "w").write(code)
    subprocess.run(["make", "clean"], cwd=d, env=ENV, capture_output=True)
    r = subprocess.run(["make"], cwd=d, env=ENV, capture_output=True, text=True, timeout=600)
    out = (r.stdout + r.stderr)
    out = "\n".join(l for l in out.splitlines() if "GNU-stack" not in l and "deprecated" not in l)
    return ("[success]" in out and "error:" not in out), out

def write_results(results):
    with open(os.path.join(HERE, "RESULTS.md"), "w") as f:
        f.write("# Physics simulations written by the local model (%s)\n\n" % MODEL)
        f.write("Checked only by compiling, plus my own review of GASBOX. NOT yet run on a calculator, so physics and controls are unverified.\n\n")
        f.write("| Program | Result | Attempts |\n|---|---|---|\n")
        for n, s, t, _ in results:
            f.write("| %s | %s | %d |\n" % (n, s, t))
        f.write("\nSource: `<NAME>/src/main.c`; built file: `<NAME>/bin/<NAME>.8xp`.\n")

def main():
    global spent
    results = []
    open(os.path.join(HERE, "run.log"), "a").write("--- started %s\n" % time.ctime())
    for name, spec in SPECS:
        if os.path.exists(os.path.join(HERE, name, "bin", name + ".8xp")):
            results.append((name, "already built (kept as is)", 0, ""))
            continue
        if spent > ACTIVE_BUDGET:
            results.append((name, "skipped (time budget)", 0, ""))
            continue
        prompt = RULES.format(skeleton=SKELETON, spec=spec)
        code, status, errors, tries = "", "failed", "", 0
        for tries in range(1, MAX_TRIES + 1):
            wait_for_gpu()
            log("%s: attempt %d" % (name, tries))
            t0 = time.time()
            try:
                text = generate(prompt)
            except Exception as e:
                log("%s: model error %s" % (name, e))
                time.sleep(60)
                continue
            spent += time.time() - t0
            code = extract_c(text)
            ok, errors = build(name, code)
            warn = errors.count("warning:")
            os.makedirs(os.path.join(HERE, name, "attempts"), exist_ok=True)
            open(os.path.join(HERE, name, "attempts", "try%d.c" % tries), "w").write(code)
            if ok:
                status = "compiles" + (" (%d warnings)" % warn if warn else "")
                break
            prompt = (RULES.format(skeleton=SKELETON, spec=spec)
                      + "\nYour previous attempt failed to compile. Here is your code:\n```c\n" + code
                      + "```\nCompiler output:\n" + errors[-2500:]
                      + "\nFix every error and output the COMPLETE corrected program in one ```c block.")
        results.append((name, status, tries, "" if status.startswith("compiles") else errors[-400:]))
        log("%s: %s after %d attempt(s)" % (name, status, tries))
        write_results(results)
    write_results(results)
    if False:
        with open(os.path.join(HERE, "RESULTS.md"), "w") as f:
            f.write("# Physics simulations written by the local model (%s)\n\n" % MODEL)
            f.write("Checked only by compiling. NOT yet run on a calculator, so physics and controls are unverified.\n\n")
            f.write("| Program | Result | Attempts |\n|---|---|---|\n")
            for n, s, t, _ in results:
                f.write("| %s | %s | %d |\n" % (n, s, t))
            f.write("\nSource: `<NAME>/src/main.c`; built file: `<NAME>/bin/<NAME>.8xp`.\n")
    for m in ("ti84coder", "qwen3.8:27b"):
        subprocess.run(["ollama", "stop", m], capture_output=True)
    good = sum(1 for r in results if r[1].startswith("compiles") or r[1].startswith("already"))
    summary = "Local model overnight job finished: %d of %d physics sims compile (see ti84/physics/RESULTS.md). Model unloaded, GPU free." % (good, len(results))
    log(summary)
    subprocess.run(["node", os.path.expanduser("~/.claude/skills/claude-check/scripts/post.mjs"), summary], capture_output=True)

if __name__ == "__main__":
    main()
