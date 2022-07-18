#!/usr/bin/python3

import sys, argparse, subprocess, pathlib, os

# import utils module
sdk_root_dir = pathlib.Path(__file__).absolute().parent.parent.parent.parent
sys.path.insert(1, f"{sdk_root_dir}/dev/script")
import utils

# setup command line arguments
ap = argparse.ArgumentParser()
ap.add_argument("-s", dest="device", help="Specify serial number of the Android device")
ap.add_argument("-r", action="store_true", help="Launch Release build instead of Debug build.")
ap.add_argument("-G", action="store_true", help="Launch Garage scene).")
ap.add_argument("-R", action="store_true", help="Launch OPPO Ring scene).")
ap.add_argument("-P", action="store_true", help="Launch PathTracerDemo scene).")
ap.add_argument("-T", action="store_true", help="Launch test activity).")
ap.add_argument("-I", action="store_true", help="Skip installation.")
args = ap.parse_args()

adb = utils.AndroidDebugBridge(args.device)
dir = os.path.dirname(os.path.realpath(__file__))
gradle = os.path.join(dir, "gradlew.bat") if os.name == "nt" else os.path.join(dir, "gradlew")

# deploy via gradle
if not args.I:
    utils.logi("Deploy to phone...")
    variant = "Release" if args.r else "Debug"
    try:
        subprocess.check_call([gradle, f"install{variant}"])
    except subprocess.CalledProcessError:
        # first deploy attempt has failed, try uninstall then deploy again
        subprocess.check_call([gradle, "uninstallAll"])
        subprocess.check_call([gradle, f"install{variant}"])

# Determine the activity
activity = "MainActivity"
if args.G: activity = "GarageActivity"
if args.R: activity = "RingActivity"
if args.P: activity = "PTDemoActivity"
if args.T: activity = "TestActivity"

# launch via gradle
suffix   = "" if args.r else ".DBG"
utils.logi(f"Launching {activity}...")
adb.run(["shell", f"am start com.innopeak.ph.sdk.sample.hub{suffix}/com.innopeak.ph.sdk.sample.hub.{activity}"], verbose=True)
