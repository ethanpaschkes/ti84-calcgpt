"""A second, independent reader of the bytes browserd.py sends to the calculator, used only by the tests to check them."""


def decode(data):
    """A second, independent reader of the bytes sent to the calculator (what BROWSER's page model must also do)."""
    msgs, i = [], 0
    while i < len(data):
        c = chr(data[i]); i += 1
        if c == "H":
            n = data[i]; i += 1; names = []
            for _ in range(n):
                names.append(data[i + 1:i + 1 + data[i]].decode()); i += 1 + data[i]
            msgs.append(("H", names))
        elif c == "B":
            kind = chr(data[i]); n = data[i + 1]; msgs.append(("B", kind, data[i + 2:i + 2 + n].decode())); i += 2 + n
        elif c == "W":
            style, ns, n = data[i], data[i + 1], data[i + 2]
            text = data[i + 3:i + 3 + n].decode(); i += 3 + n; spans = []
            for _ in range(ns):
                spans.append((data[i] | data[i + 1] << 8, data[i + 2], data[i + 3])); i += 4
            msgs.append(("W", style, text, spans))
        elif c == "E":
            msgs.append(("E",))
        elif c == "T":
            n = data[i]; msgs.append(("T", data[i + 1:i + 1 + n].decode())); i += 1 + n
        else:
            raise ValueError(f"unknown message {c!r} at {i - 1}")
    return msgs
