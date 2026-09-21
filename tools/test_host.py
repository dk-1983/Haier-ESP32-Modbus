from pathlib import Path
import os,shutil,subprocess,sys
root=Path(__file__).resolve().parents[1];out=root/'work';out.mkdir(exist_ok=True)
compiler=os.environ.get('CXX') or shutil.which('g++') or shutil.which('clang++')
command=[compiler] if compiler else [sys.executable,'-m','ziglang','c++']
binary=out/('test_modbus.exe' if os.name=='nt' else 'test_modbus')
subprocess.run(command+['-std=c++17','-Wall','-Wextra','-I',str(root),str(root/'tests/test_modbus.cpp'),'-o',str(binary)],check=True,cwd=root)
subprocess.run([str(binary)],check=True,cwd=root)

# Guard the real sensor-copy helper against overwriting neighbouring objects.
sensor_binary=out/('test_status_sensors.exe' if os.name=='nt' else 'test_status_sensors')
subprocess.run(command+['-std=c++17','-Wall','-Wextra','-I',str(root),str(root/'tests/test_status_sensors.cpp'),'-o',str(sensor_binary)],check=True,cwd=root)
subprocess.run([str(sensor_binary)],check=True,cwd=root)

management_binary=out/('test_management.exe' if os.name=='nt' else 'test_management')
subprocess.run(command+['-std=c++17','-Wall','-Wextra','-I',str(root),str(root/'tests/test_management.cpp'),'-o',str(management_binary)],check=True,cwd=root)
subprocess.run([str(management_binary)],check=True,cwd=root)
