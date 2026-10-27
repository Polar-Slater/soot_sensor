"""Build the real driver against TI-RTOS/I2C stubs, then run behavioral tests."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--compiler", nargs="+", default=["cc"],
                    help="Compiler command, e.g. --compiler gcc or --compiler zig cc")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
flags = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-I", str(root / "include"), "-I", str(root / "tests/stubs")]
with tempfile.TemporaryDirectory(prefix="soot-tests-") as build:
    binary = Path(build) / "test_soot_resistance.exe"
    subprocess.run(args.compiler + flags + [
        str(root / "src/soot_resistance.c"),
        str(root / "tests/test_soot_resistance.c"), "-lm", "-o", str(binary)],
        check=True)
    subprocess.run([str(binary)], check=True)
    subprocess.run(args.compiler + flags + ["-c",
        str(root / "examples/cc2650_tirtos_example.c"),
        "-o", str(Path(build) / "example.o")], check=True)
print("PASS: integration example compiles against legacy API stubs")
