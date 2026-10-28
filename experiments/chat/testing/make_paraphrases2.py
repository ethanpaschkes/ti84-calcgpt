#!/usr/bin/env python3
"""Ask the local model for ways a student might phrase each answer's question.

usage: make_paraphrases.py ENTRIES.tsv OUT.tsv [first_table]
Writes lines "slot<TAB>question". Safe to stop and restart: finished slots are skipped.
Needs Ollama running with qwen3.8:27b."""
import json, sys, time, urllib.request

MODEL = "qwen3.8:27b"
PER_ENTRY = 14
STYLE = ("Describe the situation or ask indirectly instead of naming the concept, as a confused student would "
         "(for example 'what happens to the balloon when its cold' rather than 'charles law'). "
         "Some messages can be a bit longer (up to 40 characters).\n")

def ask(summary):
    prompt = ("You are helping test a calculator chatbot. Students type very short messages on a calculator "
              "keyboard (usually under 30 characters, lowercase, often with typos or slang like u, r, wat).\n"
              "Write %d different short messages a student might type to get this answer from the chatbot.\n"
              "Rules: one message per line, no numbering, no quotes, no explanations. "
              "Mix plain questions, typos and slang.\n" % PER_ENTRY
              + STYLE + "\nANSWER: " + summary)
    body = json.dumps({"model": MODEL, "prompt": prompt, "stream": False, "think": False,
                       "options": {"temperature": 1.0, "num_predict": 300}}).encode()
    req = urllib.request.Request("http://localhost:11434/api/generate", body, {"Content-Type": "application/json"})
    return json.load(urllib.request.urlopen(req, timeout=900))["response"]

def clean(line):
    line = line.strip().lstrip("-*0123456789.) ").strip().strip('"\'')
    return line if 3 <= len(line) <= 40 else None

def main():
    entries_path, out_path = sys.argv[1], sys.argv[2]
    first_table = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    done = set()
    try:
        for l in open(out_path):
            done.add(l.split("\t")[0])
    except FileNotFoundError:
        pass
    rows = [l.rstrip("\n").split("\t") for l in open(entries_path)]
    rows = [r for r in rows if int(r[1]) >= first_table and r[0] not in done]
    started = time.time()
    with open(out_path, "a") as out:
        for n, (slot, table, keys, summary) in enumerate(rows, 1):
            try:
                text = ask(summary)
            except Exception as e:
                print("slot", slot, "failed:", e, flush=True)
                continue
            qs = [q for q in (clean(l) for l in text.splitlines()) if q]
            for q in qs[:PER_ENTRY]:
                out.write("%s\t%s\n" % (slot, q))
            out.flush()
            rate = (time.time() - started) / n
            print("%d/%d slot %s: %d questions (%.0fs each, about %.0f min left)" %
                  (n, len(rows), slot, len(qs), rate, rate * (len(rows) - n) / 60), flush=True)

main()
