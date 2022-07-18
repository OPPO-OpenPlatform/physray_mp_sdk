#!/usr/bin/python3

import sys, os, pathlib, subprocess, shutil, json

# import utils.py from dev/script
# import utils.py from dev/script
self_dir=pathlib.Path(os.path.dirname(__file__)).absolute()
root_dir=(self_dir / "../../..").resolve()
sys.path.insert(0, f"{root_dir}/dev/script")
import utils

# define default destination folder
dest_dir = (self_dir / "../../asset/model/garage/4.0a").absolute()
# TODO: create dest_dir if not exist

# look for ktx binary
if os.name == 'nt':
    toktx = root_dir / "dev/bin/mswin/ktx/toktx.exe"
else:
    toktx = root_dir / "dev/bin/linux/toktx"
if not toktx.is_file: utils.rip(f"{toktx} not found.")

# Function to convert regular image files to .ktx2
def convertToKTX2(source_path, mipmap = True, sRGB = True):
    dest_path = (dest_dir / source_path.stem).with_suffix(".ktx2")
    options = [
        "--encode", "astc",
        "--astc_blk_d", "8x8",
        "--t2",
        "--target_type", "RGBA",
        "--assign_oetf", "srgb" if sRGB else "linear"
    ]
    if mipmap: options += ["--genmipmap"]
    cmdline = [toktx] + options + [dest_path, source_path]
    utils.logi(' '.join([str(x) for x in cmdline]))
    subprocess.check_call(cmdline)

# Process GLTF files
def convertGLTF(source_path):
    dest_path = dest_dir / source_path.name
    utils.logi(f"Convert GLTF: {source_path} -> {dest_path}")
    gltf = {}
    with open(source_path) as source_file:
        gltf = json.load(source_file)
    
    # go through all json values recursively
    def process(table):
        if isinstance(table, dict):
            for k, v in table.items():
                if isinstance(v, dict): process(v)
                elif isinstance(v, list): process(v)
                elif k == "uri" and isinstance(v, str):
                    if ("normal" in v.lower()) or ("basecolor" in v.lower()):
                        table[k] = str(pathlib.Path(v).with_suffix(".ktx2"))
        elif isinstance(table, list):
            for v in table:
                if isinstance(v, dict): process(v)
                elif isinstance(v, list): process(v)
    process(gltf)

    with open(dest_path, "w") as dest_file:
        json.dump(gltf, dest_file, indent=4)

# Function to copy file to dest folder as is.
def copyToDestDir(source_path):
    dest_path = dest_dir / source_path.name
    utils.logi(f"Copy: {source_path} -> {dest_path}")
    shutil.copyfile(source_path, dest_path)

# grab file list
for source in os.listdir(self_dir):
    if os.path.isdir(source): continue # skip subfolders.

    source_path = pathlib.Path(source).absolute()
    source_ext  = source_path.suffix.lower()
    source_stem = source_path.stem.lower()

    # ignore certain file types
    if ".cmd" == source_ext or ".py" == source_ext: continue

    # process image files
    if ".png" == source_ext or ".jpg" == source_ext:
        if "basecolor" in source_stem:
            convertToKTX2(source_path, mipmap=True, sRGB = False) # TODO: should convert base map to sRGB.
            continue
        elif "normal" in source_stem:
            convertToKTX2(source_path, mipmap=False, sRGB = False) # do _NOT_ generate mipmap for normal map
            continue

    if ".gltf" == source_ext:
        convertGLTF(source_path)
        continue

    # For everthing else, just copy to dest folder
    copyToDestDir(source_path)
