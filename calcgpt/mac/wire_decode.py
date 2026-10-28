"""A second, independent reader of the bytes calcgptd.py sends to the calculator, used only by the tests."""


def decode(data):
    msgs, i = [], 0
    while i < len(data):
        c = chr(data[i]); i += 1
        if c in "ATL":
            n = data[i]; msgs.append((c, data[i + 1:i + 1 + n].decode())); i += 1 + n
        elif c in "DCK":
            msgs.append((c,))
        elif c == "U":
            msgs.append((c, data[i])); i += 1
        else:
            raise ValueError(f"unknown message {c!r} at {i - 1}")
    return msgs
