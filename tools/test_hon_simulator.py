"""Cross-check the bench codec against the project's installed HaierProtocol."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
library = root / '.esphome/work/build/.piolibdeps/haier-s3/HaierProtocol'
if not (library / 'include/transport/haier_frame.h').exists():
    raise SystemExit('HaierProtocol missing: build haier-s3.yaml first.')
out = root / 'work'
out.mkdir(exist_ok=True)
compiler = os.environ.get('CXX') or shutil.which('g++') or shutil.which('clang++')
command = [compiler] if compiler else [sys.executable, '-m', 'ziglang', 'c++']
binary = out / ('test_hon_simulator.exe' if os.name == 'nt' else 'test_hon_simulator')
subprocess.run(command + [
    '-std=c++17', '-Wall', '-Wextra', '-include', 'cstddef',
    '-I', str(root / 'bench'), '-I', str(library / 'include'),
    str(root / 'tests/test_hon_simulator.cpp'),
    str(library / 'src/transport/haier_frame.cpp'), '-o', str(binary),
], check=True, cwd=root)
subprocess.run([str(binary)], check=True, cwd=root)
