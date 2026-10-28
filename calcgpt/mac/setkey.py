#!/usr/bin/env python3
"""Saves your cloud key for CalcGPT: asks for it (what you paste is hidden), writes it to .env next to this file, then checks it works.
Usage:  python3 setkey.py        The key stays on this Mac in .env (readable only by you); it is never printed."""
import getpass, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import calcgptd as G


def write_key(path, key):
    """Sets OPENAI_API_KEY in the .env file, keeping any other lines, with owner-only permissions."""
    lines = [l for l in (open(path).read().splitlines() if os.path.exists(path) else []) if not l.strip().startswith("OPENAI_API_KEY")]
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write("\n".join(lines + ["OPENAI_API_KEY=" + key]) + "\n")
    os.chmod(path, 0o600)


def main():
    key = getpass.getpass("Paste your OpenAI key (Cmd+V). It will look like nothing happened: that is normal. Then press Enter: ").strip()
    if not key or any(c.isspace() or c in "\"'" for c in key):
        sys.exit("That does not look like a key (empty, or it has spaces or quote marks). Nothing was saved.")
    print(f"Got {len(key)} characters, starting with {key[:3]}")
    path = os.path.join(HERE, ".env")
    write_key(path, key)
    print("Saved to", path)
    models, _ = G.load_models()
    cloud = [m for m in models if m["provider"] != "ollama"]
    if not cloud:
        return
    try:
        names = G.list_cloud(cloud[0], key)
    except OSError as e:
        sys.exit(f"The key was saved, but the cloud did not accept it: {e}")
    print("The key works.")
    for m in cloud:
        print(f"  {m['label']}: {m['model']} ->", "available" if m["model"] in names else "NOT on your account, pick another name below and edit models.json")
    small = [n for n in names if "mini" in n or "nano" in n]
    print("Fast-looking names on your account:", ", ".join(small[:12]) or "(none found)")


if __name__ == "__main__":
    main()
