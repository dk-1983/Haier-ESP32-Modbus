from pathlib import Path
import os,shutil,subprocess,sys
root=Path(__file__).resolve().parents[1];out=root/'work';out.mkdir(exist_ok=True)
compiler=os.environ.get('CXX') or shutil.which('g++') or shutil.which('clang++')
command=[compiler] if compiler else [sys.executable,'-m','ziglang','c++']
binary=out/('test_modbus.exe' if os.name=='nt' else 'test_modbus')
subprocess.run(command+['-std=c++17','-Wall','-Wextra','-I',str(root),str(root/'tests/test_modbus.cpp'),'-o',str(binary)],check=True,cwd=root)
subprocess.run([str(binary)],check=True,cwd=root)
