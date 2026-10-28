"""Compile the ESP32 driver with Wire stubs; run behavior and transfer tests."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--compiler", nargs="+", default=["c++"],
                    help="C++ compiler command, e.g. g++ or zig c++")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
flags = ["-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-I", str(root / "src"), "-I", str(root / "tests/stubs")]
with tempfile.TemporaryDirectory(prefix="soot-esp32-tests-") as build:
    binary = Path(build) / "test_soot_resistance.exe"
    subprocess.run(args.compiler + flags + [
        str(root / "src/soot_resistance.cpp"),
        str(root / "tests/test_soot_resistance.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    subprocess.run(args.compiler + flags + ["-x", "c++", "-c",
        str(root / "examples/ResistanceMonitor/ResistanceMonitor.ino"),
        "-o", str(Path(build) / "example.o")], check=True)
print("PASS: ESP32 example compiles against Arduino/Wire stubs")
