#!/usr/bin/python3

from email import utils
import sys, os, pathlib, subprocess, pprint

sdk_root = pathlib.Path(__file__).absolute().parent.parent.parent.parent
sys.path.insert(1, f"{sdk_root}/dev/script")
import utils

newenv = os.environ.copy()
newenv["PH_SAVE_BVH"] = "1"

def run(program, argv = []):
    exe = utils.search_for_the_latest_binary(f"{sdk_root}/build/sample/desktop/{{variant}}/{program}")
    if not exe: return
    cmdline = [str(exe), "-oQ", "--max-frames", "1"] + argv
    print(" ".join(cmdline))
    subprocess.run(cmdline, env = newenv)

run("suzanne")
run("suzanne", ["--glasses"])
run("suzanne", ["--helmet"])
run("suzanne", ["--rocket"])
run("ring")
run("shadow")
run("garage")