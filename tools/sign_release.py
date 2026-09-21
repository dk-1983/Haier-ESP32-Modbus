"""Sign a verified public Haier OTA image; private P-256 key stays outside Git."""
import argparse, hashlib, json, re
from pathlib import Path
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import ec
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True);p.add_argument('--key',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();meta=json.loads((a.build/'build-info.json').read_text());v=meta['version']
if not re.fullmatch(r'[0-9]{1,5}\.[0-9]{1,5}\.[0-9]{1,5}',v):raise SystemExit('Invalid stable version')
name=f'haier-s3-n16r8-v{v}-ota.bin';data=(a.build/name).read_bytes()
if not meta.get('public_provisioning') or meta['target']!='ESP32-S3-WROOM-1 N16R8':raise SystemExit('Not a public provisioning build')
if len(data)<128 or len(data)>0x7c0000 or data[0]!=0xe9 or data[12:14]!=b'\x09\x00' or data[32:36]!=b'\x32\x54\xcd\xab':raise SystemExit('Invalid app image')
payload={'schema':1,'channel':'stable','profile':'haier-s3-n16r8-v1','version':v,'url':f'https://github.com/dk-1983/Haier-ESP32-Modbus/releases/download/v{v}/{name}','size':len(data),'sha256':hashlib.sha256(data).hexdigest()}
raw=json.dumps(payload,separators=(',',':'),sort_keys=True)
key=serialization.load_pem_private_key(a.key.read_bytes(),password=None)
if not isinstance(key,ec.EllipticCurvePrivateKey) or not isinstance(key.curve,ec.SECP256R1):raise SystemExit('Expected P-256 key')
sig=key.sign(raw.encode(),ec.ECDSA(hashes.SHA256()));key.public_key().verify(sig,raw.encode(),ec.ECDSA(hashes.SHA256()))
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps({'payload':raw,'signature':sig.hex()},indent=2)+'\n',encoding='utf-8')
print('Signed',v,'SHA256',payload['sha256'])
