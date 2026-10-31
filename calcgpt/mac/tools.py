"""The tools ChatGPT can call while answering on the calculator (OpenAI function calling).

Two kinds:
  SERVER tools run here in Python and give the model exact numbers: a calculator for expressions, solvers (triangle, quadratic,
  linear, 2x2 system), statistics, trig values with exact forms, number facts, fractions, unit conversion, points geometry, the date.
  CALCULATOR tools happen on the calculator's screen: a form to fill in (ask_form), a list to pick from (ask_choice), an answers
  card, a steps card, drawn diagrams (triangle, parabola, lines, unit circle, dot plot), the decimals setting, the calculator's status.
  A request travels as a status message whose text starts with byte 1 (so the box firmware passes it through unchanged):
  "form|Title|hint|a|b|c"; the calculator answers with a question message that also starts with byte 1: "form|a=3|b=4".
  Protocol: wireless/PROTOCOL.md section 5. The calculator side: calcgpt/src/tools.c.

No packages needed. Tested by test_tools.py."""
import ast, datetime, json, math, operator, statistics, time, unicodedata, zoneinfo

MARK = "\x01"            # the first byte of a tool message, both ways
PAYLOAD_MAX = 254        # one status message holds 255 bytes: the marker and up to 254 of text
MAX_ROUNDS = 8           # tool calls answered per question
CALC_WAIT = 180.0        # seconds a form or a choice may stay on the calculator's screen
TIMEZONE = "America/New_York"


def num(x):
    """A float, with ints shown as ints when written back."""
    return float(x)


def tidy(v, places=6):
    """Rounded, and an int when it is whole, so the model reads 5 and not 5.000000001."""
    if isinstance(v, bool) or v is None:
        return v
    if isinstance(v, (int, float)):
        if v != v or v in (float("inf"), float("-inf")):
            return str(v)
        r = round(v, places)
        return int(r) if r == int(r) and abs(r) < 1e15 else r
    if isinstance(v, dict):
        return {k: tidy(x, places) for k, x in v.items()}
    if isinstance(v, (list, tuple)):
        return [tidy(x, places) for x in v]
    return v


# ---------------------------------------------------------------- the safe calculator

OPS = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul, ast.Div: operator.truediv, ast.Pow: operator.pow,
       ast.Mod: operator.mod, ast.USub: operator.neg, ast.UAdd: operator.pos, ast.FloorDiv: operator.floordiv}


def _functions(degrees):
    k = math.pi / 180 if degrees else 1
    return {
        "sqrt": math.sqrt, "abs": abs, "ln": math.log, "log": math.log10, "log2": math.log2, "exp": math.exp,
        "sin": lambda x: math.sin(x * k), "cos": lambda x: math.cos(x * k), "tan": lambda x: math.tan(x * k),
        "asin": lambda x: math.asin(x) / k, "acos": lambda x: math.acos(x) / k, "atan": lambda x: math.atan(x) / k,
        "floor": math.floor, "ceil": math.ceil, "round": round, "factorial": lambda n: math.factorial(int(n)),
        "gcd": lambda a, b: math.gcd(int(a), int(b)), "min": min, "max": max, "nCr": lambda n, r: math.comb(int(n), int(r)),
        "nPr": lambda n, r: math.perm(int(n), int(r)),
    }


def calculate(expression, angle_unit="deg"):
    """Works out an arithmetic expression safely: + - * / ^ ( ), sqrt sin cos tan asin acos atan ln log exp abs factorial nCr nPr,
    pi and e. Trig in degrees unless angle_unit is rad."""
    text = str(expression).replace("^", "**").replace("×", "*").replace("÷", "/").replace("π", "pi").replace("√", "sqrt")
    funcs, consts = _functions(angle_unit != "rad"), {"pi": math.pi, "e": math.e}
    try:
        tree = ast.parse(text.strip(), mode="eval")
    except SyntaxError as e:
        return {"error": f"could not read the expression: {e.msg}"}

    def ev(n):
        if isinstance(n, ast.Expression):
            return ev(n.body)
        if isinstance(n, ast.Constant) and isinstance(n.value, (int, float)) and not isinstance(n.value, bool):
            return n.value
        if isinstance(n, ast.Name) and n.id in consts:
            return consts[n.id]
        if isinstance(n, ast.BinOp) and type(n.op) in OPS:
            return OPS[type(n.op)](ev(n.left), ev(n.right))
        if isinstance(n, ast.UnaryOp) and type(n.op) in OPS:
            return OPS[type(n.op)](ev(n.operand))
        if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id in funcs and not n.keywords:
            return funcs[n.func.id](*[ev(a) for a in n.args])
        raise ValueError("only numbers, + - * / ^, brackets and the listed functions are allowed")

    try:
        value = ev(tree)
    except ZeroDivisionError:
        return {"error": "division by zero"}
    except (ValueError, TypeError, OverflowError) as e:
        return {"error": str(e) or "could not work that out"}
    if isinstance(value, complex):
        return {"error": "the result is not a real number"}
    return {"expression": expression, "value": value}


# ---------------------------------------------------------------- solvers

def _tri_core(s, a):
    """s, a: lists of 3 values or None (sides a b c, angles A B C in degrees). Returns (solutions, rule) or raises ValueError."""
    sind, cosd = (lambda d: math.sin(math.radians(d))), (lambda d: math.cos(math.radians(d)))
    asind, acosd = (lambda v: math.degrees(math.asin(max(-1.0, min(1.0, v))))), (lambda v: math.degrees(math.acos(max(-1.0, min(1.0, v)))))
    ns, na = sum(x is not None for x in s), sum(x is not None for x in a)
    if ns + na != 3:
        raise ValueError(f"exactly three values are needed (got {ns + na}); at least one must be a side")
    if ns == 0:
        raise ValueError("angles alone do not fix the size: give at least one side")
    s, a = list(s), list(a)
    if ns == 3:
        big = max(range(3), key=lambda i: s[i])
        p, q = (big + 1) % 3, (big + 2) % 3
        if s[p] + s[q] <= s[big]:
            raise ValueError(f"no triangle: {s[p]} + {s[q]} is not more than {s[big]}, the two short sides can't meet")
        for i in range(2):
            y, z, x = s[(i + 1) % 3], s[(i + 2) % 3], s[i]
            a[i] = acosd((y * y + z * z - x * x) / (2 * y * z))
        a[2] = 180 - a[0] - a[1]
        return [(s, a)], "Law of Cosines (SSS)"
    if na == 2:
        miss = a.index(None)
        total = sum(x for x in a if x is not None)
        if total >= 180:
            raise ValueError(f"no triangle: the two angles add to {total}, which is 180 or more")
        a[miss] = 180 - total
        ks = next(i for i in range(3) if s[i] is not None)
        k = s[ks] / sind(a[ks])
        for i in range(3):
            if s[i] is None:
                s[i] = k * sind(a[i])
        return [(s, a)], "Law of Sines (ASA/AAS)"
    ai = next(i for i in range(3) if a[i] is not None)
    p, q = (ai + 1) % 3, (ai + 2) % 3
    if s[p] is not None and s[q] is not None:
        x, y = s[p], s[q]
        s[ai] = math.sqrt(x * x + y * y - 2 * x * y * cosd(a[ai]))
        small = p if x <= y else q
        a[small] = asind(s[small] * sind(a[ai]) / s[ai])
        a[3 - ai - small] = 180 - a[ai] - a[small]
        return [(s, a)], "Law of Cosines, then Law of Sines (SAS)"
    other = p if s[p] is not None else q
    third = 3 - ai - other
    sy = s[other] * sind(a[ai]) / s[ai]
    if sy > 1 + 1e-9:
        raise ValueError(f"no triangle: sin {'ABC'[other]} would be {sy:.4g}, more than 1 (side {'abc'[ai]} is too short to reach)")
    sy = min(sy, 1.0)
    y1 = asind(sy)
    sols = []
    for yv in ([y1] if abs(sy - 1) < 1e-9 else [y1, 180 - y1]):
        z = 180 - a[ai] - yv
        if z <= 1e-9:
            continue
        ss, aa = list(s), list(a)
        aa[other], aa[third] = yv, z
        ss[third] = s[ai] * sind(z) / sind(a[ai])
        sols.append((ss, aa))
    if not sols:
        raise ValueError("no triangle: the angles would add to more than 180")
    return sols, "Law of Sines (SSA, the ambiguous case)" if len(sols) == 2 else "Law of Sines (SSA)"


def solve_triangle(a=None, b=None, c=None, A=None, B=None, C=None, right=False):
    """Solves a triangle from any three values (sides a b c opposite angles A B C, degrees); right=true means C = 90 and two
    values are enough. Gives both triangles in the ambiguous SSA case."""
    s = [None if v is None else num(v) for v in (a, b, c)]
    ang = [None if v is None else num(v) for v in (A, B, C)]
    if right:
        if ang[2] is not None and abs(ang[2] - 90) > 1e-6:
            return {"error": "in a right triangle C is the 90 degree angle"}
        ang[2] = 90.0
    for i, v in enumerate(s):
        if v is not None and v <= 0:
            return {"error": f"side {'abc'[i]} must be more than 0"}
    for i, v in enumerate(ang):
        if v is not None and not 0 < v < 180:
            return {"error": f"angle {'ABC'[i]} must be between 0 and 180"}
    try:
        sols, rule = _tri_core(s, ang)
    except ValueError as e:
        return {"error": str(e)}
    out = []
    for ss, aa in sols:
        out.append({"a": ss[0], "b": ss[1], "c": ss[2], "A": aa[0], "B": aa[1], "C": aa[2],
                    "area": 0.5 * ss[0] * ss[1] * math.sin(math.radians(aa[2])), "perimeter": sum(ss)})
    if right and rule.startswith("Law of Cosines, then") or right and "SSA" in rule:
        rule = "Pythagoras and SOH CAH TOA"
    return tidy({"rule": rule, "solutions": out, "count": len(out)})


def solve_quadratic(a, b, c):
    """Solves a x^2 + b x + c = 0: real or complex roots, the discriminant and the vertex."""
    a, b, c = num(a), num(b), num(c)
    if a == 0:
        if b == 0:
            return {"error": "a and b are both 0: there is no x"}
        return tidy({"note": "a is 0, so it is linear", "roots": [-c / b]})
    disc = b * b - 4 * a * c
    vx = -b / (2 * a)
    out = {"discriminant": disc, "vertex": {"x": vx, "y": a * vx * vx + b * vx + c}, "opens": "up" if a > 0 else "down"}
    if disc < 0:
        re, im = -b / (2 * a), math.sqrt(-disc) / (2 * abs(a))
        out.update(real_roots=0, complex_roots=[f"{tidy(re)} + {tidy(im)}i", f"{tidy(re)} - {tidy(im)}i"])
    else:
        r = math.sqrt(disc)
        roots = sorted({(-b - r) / (2 * a), (-b + r) / (2 * a)})
        out.update(real_roots=len(roots), roots=roots)
        if len(roots) == 1:
            out["note"] = "a double root: the parabola touches the x axis"
    return tidy(out)


def solve_linear(a, b, c=0, d=0):
    """Solves a x + b = c x + d for x (c and d default to 0, so a x + b = 0 works too)."""
    a, b, c, d = num(a), num(b), num(c), num(d)
    if a == c:
        return {"solutions": "every x" if b == d else "none", "why": "the x terms cancel"}
    x = (d - b) / (a - c)
    return tidy({"x": x, "check_left": a * x + b, "check_right": c * x + d})


def solve_system(a1, b1, c1, a2, b2, c2):
    """Solves a1 x + b1 y = c1 and a2 x + b2 y = c2 (Cramer's rule)."""
    a1, b1, c1, a2, b2, c2 = (num(v) for v in (a1, b1, c1, a2, b2, c2))
    det = a1 * b2 - a2 * b1
    if det == 0:
        same = a1 * c2 == a2 * c1 and b1 * c2 == b2 * c1
        return {"solutions": "infinitely many (the same line)" if same else "none (parallel lines)", "determinant": 0}
    return tidy({"x": (c1 * b2 - c2 * b1) / det, "y": (a1 * c2 - a2 * c1) / det, "determinant": det})


def stats(numbers):
    """Mean, median, mode, range, quartiles, sample and population standard deviation of a list of numbers."""
    xs = [num(v) for v in numbers]
    if not xs:
        return {"error": "the list is empty"}
    xs_sorted = sorted(xs)
    out = {"n": len(xs), "sum": sum(xs), "mean": statistics.fmean(xs), "median": statistics.median(xs), "min": xs_sorted[0],
           "max": xs_sorted[-1], "range": xs_sorted[-1] - xs_sorted[0], "sorted": xs_sorted}
    modes = statistics.multimode(xs)
    out["mode"] = "none (every value once)" if len(modes) == len(set(xs)) and len(xs) > 1 else modes
    if len(xs) > 1:
        out.update(sample_sd=statistics.stdev(xs), population_sd=statistics.pstdev(xs), variance=statistics.variance(xs))
        if len(xs) >= 4:
            q = statistics.quantiles(xs, n=4)
            out.update(q1=q[0], q3=q[2], iqr=q[2] - q[0])
    return tidy(out)


EXACT = [(0, "0"), (0.5, "1/2"), (math.sqrt(2) / 2, "sqrt(2)/2"), (math.sqrt(3) / 2, "sqrt(3)/2"), (1, "1"),
         (math.sqrt(3) / 3, "sqrt(3)/3"), (math.sqrt(3), "sqrt(3)")]


def exact_form(v):
    for value, name in EXACT:
        if abs(abs(v) - value) < 1e-9:
            return ("-" if v < -1e-12 else "") + name
    return None


def trig_values(angle, unit="deg"):
    """sin, cos, tan of an angle (degrees unless unit is rad) with the exact unit-circle forms, the quadrant, the reference angle
    and the angle in radians as a multiple of pi."""
    deg = num(angle) if unit != "rad" else math.degrees(num(angle))
    d = deg % 360
    s, c = math.sin(math.radians(d)), math.cos(math.radians(d))
    s, c = (0.0 if abs(s) < 1e-12 else s), (0.0 if abs(c) < 1e-12 else c)
    out = {"angle_deg": d, "sin": s, "cos": c, "tan": "undefined" if c == 0 else s / c}
    for k in ("sin", "cos", "tan"):
        if isinstance(out[k], float) and exact_form(out[k]):
            out[k + "_exact"] = exact_form(out[k])
    on_axis = abs(d % 90) < 1e-9 or abs(d % 90 - 90) < 1e-9
    out["quadrant"] = "on an axis" if on_axis else int(d // 90) + 1
    if not on_axis:
        q = out["quadrant"]
        out["reference_angle"] = {1: d, 2: 180 - d, 3: d - 180, 4: 360 - d}[q]
    for den in range(1, 13):
        n = round(d * den / 180)
        if abs(n / den * 180 - d) < 1e-6:
            out["radians"] = "0" if n == 0 else (("" if n == 1 else str(n)) + "pi" + (f"/{den}" if den > 1 else ""))
            break
    else:
        out["radians"] = math.radians(d)
    return tidy(out)


def number_facts(n):
    """Prime or not, prime factors, divisors, odd/even, perfect square of a whole number."""
    try:
        v = int(float(n))
    except (TypeError, ValueError):
        return {"error": "not a whole number"}
    if v != float(n) or v < 1 or v > 10**12:
        return {"error": "give a whole number from 1 to 1000000000000"}
    factors, m, p = [], v, 2
    while p * p <= m:
        while m % p == 0:
            factors.append(p)
            m //= p
        p += 1 if p == 2 else 2
    if m > 1:
        factors.append(m)
    out = {"n": v, "prime": v > 1 and factors == [v], "prime_factors": factors, "even": v % 2 == 0,
           "perfect_square": int(math.isqrt(v)) ** 2 == v}
    if v <= 10**6:
        out["divisors"] = [d for d in range(1, v + 1) if v % d == 0] if v <= 10000 else f"{sum(1 for d in range(1, int(math.isqrt(v)) + 1) if v % d == 0) * 2 - (1 if int(math.isqrt(v)) ** 2 == v else 0)} divisors"
    return out


def fraction(numerator=None, denominator=None, decimal=None):
    """Simplifies numerator/denominator, or turns a decimal into the closest simple fraction; gives the mixed number and the decimal."""
    from fractions import Fraction
    try:
        if decimal is not None:
            f = Fraction(str(decimal)).limit_denominator(1000)
        else:
            f = Fraction(Fraction(str(numerator)), Fraction(str(denominator)))
    except (ValueError, ZeroDivisionError, TypeError):
        return {"error": "could not read the fraction (is the denominator 0?)"}
    whole, rest = divmod(abs(f.numerator), f.denominator)
    mixed = f"{'-' if f < 0 else ''}{whole} {rest}/{f.denominator}" if whole and rest else str(f)
    return {"fraction": f"{f.numerator}/{f.denominator}" if f.denominator != 1 else str(f.numerator), "mixed": mixed, "decimal": tidy(float(f))}


UNITS = {
    "length": {"mm": 0.001, "cm": 0.01, "m": 1.0, "km": 1000.0, "in": 0.0254, "ft": 0.3048, "yd": 0.9144, "mi": 1609.344, "nmi": 1852.0},
    "mass": {"mg": 1e-6, "g": 0.001, "kg": 1.0, "oz": 0.028349523125, "lb": 0.45359237, "ton": 907.18474, "tonne": 1000.0},
    "time": {"s": 1.0, "min": 60.0, "h": 3600.0, "day": 86400.0, "week": 604800.0},
    "speed": {"m/s": 1.0, "km/h": 1 / 3.6, "mph": 0.44704, "ft/s": 0.3048, "knot": 0.514444},
    "volume": {"ml": 0.001, "l": 1.0, "gal": 3.785411784, "qt": 0.946352946, "cup": 0.2365882365, "floz": 0.0295735295625},
    "area": {"cm2": 1e-4, "m2": 1.0, "km2": 1e6, "ft2": 0.09290304, "acre": 4046.8564224, "ha": 10000.0},
}
ALIASES = {"meter": "m", "metre": "m", "meters": "m", "kilometer": "km", "kilometers": "km", "inch": "in", "inches": "in",
           "feet": "ft", "foot": "ft", "yard": "yd", "yards": "yd", "mile": "mi", "miles": "mi", "gram": "g", "grams": "g",
           "kilogram": "kg", "kilograms": "kg", "pound": "lb", "pounds": "lb", "lbs": "lb", "ounce": "oz", "ounces": "oz",
           "second": "s", "seconds": "s", "sec": "s", "minute": "min", "minutes": "min", "hour": "h", "hours": "h", "hr": "h",
           "days": "day", "weeks": "week", "kph": "km/h", "kmh": "km/h", "knots": "knot", "liter": "l", "litre": "l", "liters": "l",
           "gallon": "gal", "gallons": "gal", "cups": "cup", "celsius": "c", "fahrenheit": "f", "kelvin": "k", "degc": "c", "degf": "f",
           "centimeter": "cm", "centimeters": "cm", "millimeter": "mm", "millimeters": "mm", "nauticalmile": "nmi", "acres": "acre"}


def convert_units(value, from_unit, to_unit):
    """Converts length, mass, time, speed, volume, area and temperature (c, f, k)."""
    v = num(value)
    f, t = (ALIASES.get(u.strip().lower().replace(" ", ""), u.strip().lower().replace(" ", "")) for u in (str(from_unit), str(to_unit)))
    if f in ("c", "f", "k") and t in ("c", "f", "k"):
        kelvin = {"c": v + 273.15, "f": (v - 32) * 5 / 9 + 273.15, "k": v}[f]
        return tidy({"value": {"c": kelvin - 273.15, "f": (kelvin - 273.15) * 9 / 5 + 32, "k": kelvin}[t], "unit": t.upper()})
    for kind, table in UNITS.items():
        if f in table and t in table:
            return tidy({"value": v * table[f] / table[t], "unit": t, "kind": kind})
    return {"error": f"cannot convert {from_unit} to {to_unit}; known: " + ", ".join(", ".join(t) for t in UNITS.values()) + ", c, f, k"}


def points(x1, y1, x2, y2):
    """Distance, midpoint, slope and the line through two points."""
    x1, y1, x2, y2 = num(x1), num(y1), num(x2), num(y2)
    out = {"distance": math.hypot(x2 - x1, y2 - y1), "midpoint": [(x1 + x2) / 2, (y1 + y2) / 2]}
    if x1 == x2:
        out.update(slope="undefined (vertical)", line=f"x = {tidy(x1)}")
    else:
        m = (y2 - y1) / (x2 - x1)
        out.update(slope=m, y_intercept=y1 - m * x1, line=f"y = {tidy(m)}x + {tidy(y1 - m * x1)}")
    return tidy(out)


def date_time(timezone=TIMEZONE):
    """The current date and time (default US Eastern), and the day of the week."""
    try:
        now = datetime.datetime.now(zoneinfo.ZoneInfo(timezone))
    except (zoneinfo.ZoneInfoNotFoundError, ValueError):
        return {"error": f"unknown timezone {timezone}"}
    return {"date": now.strftime("%Y-%m-%d"), "time": now.strftime("%H:%M"), "weekday": now.strftime("%A"), "timezone": timezone}


def days_between(date1, date2):
    """Days from date1 to date2 (YYYY-MM-DD), and the weekday of each."""
    try:
        d1, d2 = (datetime.date.fromisoformat(str(d)) for d in (date1, date2))
    except ValueError:
        return {"error": "dates must be YYYY-MM-DD"}
    return {"days": (d2 - d1).days, "weeks": tidy((d2 - d1).days / 7), "weekday1": d1.strftime("%A"), "weekday2": d2.strftime("%A")}


def _p(**props):
    return {"type": "object", "properties": props, "required": [k for k, v in props.items() if not v.get("optional")]}


def _n(desc, optional=False):
    d = {"type": "number", "description": desc}
    if optional:
        d["optional"] = True
    return d


def _s(desc, optional=False):
    d = {"type": "string", "description": desc}
    if optional:
        d["optional"] = True
    return d


def _list(desc, item="string"):
    return {"type": "array", "items": {"type": item}, "description": desc}


def _schema(name, desc, params):
    for v in params["properties"].values():
        v.pop("optional", None)
    return {"type": "function", "function": {"name": name, "description": desc, "parameters": params}}


SERVER_TOOLS = {
    "calculate": (calculate, "Work out an arithmetic expression exactly (use this instead of doing arithmetic in your head): + - * / ^, brackets, sqrt sin cos tan asin acos atan ln log exp abs factorial nCr nPr, pi, e. Trig in degrees unless angle_unit is rad.",
                  _p(expression=_s("e.g. sqrt(3^2+4^2) or 2*sin(30)"), angle_unit=_s("deg (default) or rad", True))),
    "solve_triangle": (solve_triangle, "Solve a triangle from any three of a, b, c (sides) and A, B, C (angles in degrees, A opposite a). right=true means C=90 and two values suffice. Returns every side, angle, the area, the rule used, and both triangles in the ambiguous SSA case.",
                       _p(a=_n("side a", True), b=_n("side b", True), c=_n("side c", True), A=_n("angle A in degrees", True), B=_n("angle B", True), C=_n("angle C", True),
                          right={"type": "boolean", "description": "true for a right triangle with the right angle at C", "optional": True})),
    "solve_quadratic": (solve_quadratic, "Solve a x^2 + b x + c = 0: roots (real or complex), discriminant, vertex.", _p(a=_n("a"), b=_n("b"), c=_n("c"))),
    "solve_linear": (solve_linear, "Solve a x + b = c x + d for x (c, d default 0).", _p(a=_n("a"), b=_n("b"), c=_n("c", True), d=_n("d", True))),
    "solve_system": (solve_system, "Solve two equations a1 x + b1 y = c1 and a2 x + b2 y = c2.",
                     _p(a1=_n("a1"), b1=_n("b1"), c1=_n("c1"), a2=_n("a2"), b2=_n("b2"), c2=_n("c2"))),
    "stats": (stats, "Statistics of a list of numbers: mean, median, mode, range, quartiles, standard deviation.", _p(numbers=_list("the numbers", "number"))),
    "trig_values": (trig_values, "sin, cos, tan of an angle with exact unit-circle forms, quadrant, reference angle, radians.", _p(angle=_n("the angle"), unit=_s("deg (default) or rad", True))),
    "number_facts": (number_facts, "Prime test, prime factors, divisors, even/odd, perfect square of a whole number.", _p(n=_n("a whole number"))),
    "fraction": (fraction, "Simplify a fraction, or turn a decimal into a fraction; gives the mixed number too.",
                 _p(numerator=_n("numerator", True), denominator=_n("denominator", True), decimal=_n("a decimal to turn into a fraction", True))),
    "convert_units": (convert_units, "Convert length, mass, time, speed, volume, area or temperature (units like km, mi, kg, lb, h, km/h, mph, l, gal, m2, c, f, k).",
                      _p(value=_n("the value"), from_unit=_s("from"), to_unit=_s("to"))),
    "points": (points, "Distance, midpoint, slope and line through two points.", _p(x1=_n("x1"), y1=_n("y1"), x2=_n("x2"), y2=_n("y2"))),
    "date_time": (date_time, "The current date, time and weekday (US Eastern unless a timezone is given).", _p(timezone=_s("e.g. Europe/Paris", True))),
    "days_between": (days_between, "Days between two dates YYYY-MM-DD.", _p(date1=_s("from"), date2=_s("to"))),
}

CALC_TOOLS = {
    "ask_form": ("form", ["title", "hint", "fields"], "Show a form on the calculator for the user to type numbers into (up to 10 short field labels like a, b, c, A, B, C or x1, x2). Use it whenever you need several numbers from the user. Returns the values they typed (empty fields are left out), or 'cancel'.",
                 _p(title=_s("short title, under 30 characters"), hint=_s("one short line saying what to fill, under 36 characters"), fields=_list("field labels, up to 4 characters each"))),
    "ask_choice": ("choice", ["title", "options"], "Show a list on the calculator for the user to pick one option from (up to 8 options, under 24 characters each). Returns the chosen option or 'cancel'.",
                   _p(title=_s("the question, under 28 characters"), options=_list("the options"))),
    "show_answers": ("answers", ["title", "rule", "lines"], "Show an answers card on the calculator: up to 8 short lines (under 19 characters each, like 'c = 5') and the rule used.",
                     _p(title=_s("e.g. Answers are:"), rule=_s("the rule or method, under 36 characters"), lines=_list("the answer lines"))),
    "show_steps": ("steps", ["title", "lines"], "Show a worked-steps card on the calculator: up to 12 short lines with the numbers plugged in.",
                   _p(title=_s("the rule, under 36 characters"), lines=_list("the steps, under 36 characters each"))),
    "draw_triangle": ("triangle", ["a", "b", "c", "A", "B", "C"], "Draw a solved triangle to scale on the calculator with its sides and angles labelled (give all six values, from solve_triangle).",
                      _p(a=_n("a"), b=_n("b"), c=_n("c"), A=_n("A degrees"), B=_n("B"), C=_n("C"))),
    "draw_parabola": ("parabola", ["a", "b", "c"], "Draw y = a x^2 + b x + c on the calculator with its roots and vertex marked, and the solution lines.", _p(a=_n("a"), b=_n("b"), c=_n("c"))),
    "draw_lines": ("lines", ["a", "b", "c", "d"], "Draw y = a x + b and y = c x + d on the calculator with their crossing marked (the solution of a x + b = c x + d).", _p(a=_n("a"), b=_n("b"), c=_n("c"), d=_n("d"))),
    "draw_unit_circle": ("circle", ["angle"], "Draw the unit circle on the calculator with this angle (degrees) shaded and its sin, cos, tan listed with exact forms.", _p(angle=_n("degrees"))),
    "draw_dot_plot": ("dots", ["numbers"], "Draw a dot plot of up to 10 numbers on the calculator with the mean marked, and list mean, median, mode, spread.", _p(numbers=_list("the numbers", "number"))),
    "set_decimals": ("decimals", ["n"], "Set how many decimals the calculator's cards show (0 to 4).", _p(n=_n("decimals"))),
    "calculator_status": ("status", [], "The calculator's clock time, battery level and whether it is online.", _p()),
}


def schemas(calc=True):
    """The tool list for the chat request: the server tools, plus the calculator ones when the calculator supports them."""
    out = [_schema(name, desc, dict(params)) for name, (_, desc, params) in SERVER_TOOLS.items()]
    if calc:
        out += [_schema(name, desc, dict(params)) for name, (_, _, desc, params) in CALC_TOOLS.items()]
    return out


def run_server_tool(name, args):
    """Runs a server tool; always returns a dict (an error one for a bad call)."""
    fn = SERVER_TOOLS[name][0]
    try:
        result = fn(**args)
    except TypeError as e:
        result = {"error": f"bad arguments: {e}"}
    except (ValueError, OverflowError, ArithmeticError) as e:
        result = {"error": str(e) or "could not work that out"}
    return result


def _clean(s):
    """Text for the calculator: ASCII, no | (the separator) and no control bytes."""
    s = unicodedata.normalize("NFKD", str(s)).encode("ascii", "ignore").decode("ascii")
    return "".join(ch if 32 <= ord(ch) < 127 and ch != "|" else " " for ch in s).strip()


def _fmt(v):
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, (int, float)):
        return str(tidy(v, 4))
    return _clean(v)


def calc_payload(name, args):
    """The text sent to the calculator for a calculator tool (without the marker), cut to fit one message; None for an unknown tool."""
    if name not in CALC_TOOLS:
        return None
    word, order = CALC_TOOLS[name][0], CALC_TOOLS[name][1]
    parts = [word]
    for key in order:
        v = args.get(key)
        if isinstance(v, (list, tuple)):
            parts += [_fmt(x) for x in v]
        elif v is None:
            parts.append("")
        else:
            parts.append(_fmt(v))
    text = "|".join(parts)
    while len(text) > PAYLOAD_MAX and len(parts) > 2:
        parts.pop()
        text = "|".join(parts)
    return text[:PAYLOAD_MAX]


def parse_calc_reply(text):
    """What the calculator answered, as a dict for the model."""
    parts = text.split("|")
    kind = parts[0]
    if kind == "form":
        if parts[1:] == ["cancel"]:
            return {"result": "the user cancelled the form"}
        values = {}
        for p in parts[1:]:
            if "=" in p:
                k, v = p.split("=", 1)
                try:
                    values[k] = float(v)
                except ValueError:
                    values[k] = v
        return {"values": tidy(values)} if values else {"result": "the user sent the form empty"}
    if kind == "choice":
        return {"result": "the user cancelled"} if parts[1:] == ["cancel"] else {"chosen": "|".join(parts[1:])}
    if kind == "status":
        return {k: v for k, v in (p.split("=", 1) for p in parts[1:] if "=" in p)}
    if kind == "error":
        return {"error": "|".join(parts[1:])}
    return {"result": text}


def tool_message(payload):
    """The wire message that carries a calculator tool request: a status whose text starts with the marker."""
    body = (MARK + payload).encode("ascii", "replace")[:255]
    return b"T" + bytes([len(body)]) + body


TOOLS_PROMPT = (
    "\nTOOLS: you have tools. Use calculate for any arithmetic and the solve_/stats/trig tools for exact answers; never guess numbers. "
    "{calc}Keep your own text short: the cards show the numbers, you explain the idea in 1-3 lines.")
CALC_PROMPT = (
    "You can also act on the calculator's screen: ask_form collects numbers (use it instead of asking the user to type several "
    "numbers in a sentence), ask_choice offers options, show_answers / show_steps / draw_triangle / draw_parabola / draw_lines / "
    "draw_unit_circle / draw_dot_plot put cards and diagrams on the screen. For a maths problem the user names without numbers "
    "(a triangle problem, a quadratic, stats): ask_choice what kind if needed, ask_form for the values, solve with a tool, then draw "
    "or show the answers, then explain briefly. ")


def prompt_addition(calc):
    return TOOLS_PROMPT.format(calc=CALC_PROMPT if calc else "")
