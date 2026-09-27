"""Validate discovery JSON and templates without a broker (requires Jinja2)."""
from pathlib import Path
import json,re
from jinja2 import Environment,StrictUndefined
r=Path(__file__).resolve().parents[1]
s=(r/'components/fourvrs_portal/Discovery.h').read_text()
a=[json.loads(v.replace('@BASE@','haier/test').replace('@ID@','haier_test').replace('@IP@','192.0.2.1').replace('@VERSION@','0.6.0')) for v in re.findall(r'R"JSON\((.*?)\)JSON"',s)]
assert len(a)==8
samples={'available':True,'mode':'HEAT_COOL','fan':'HIGH','swing':'BOTH','preset':'NONE','target_temperature':22,'current_temperature':23.5,'quiet':False,'display':True,'vertical_position':'CENTER','horizontal_position':'CENTER'}
e=Environment(undefined=StrictUndefined)
for entity in a[:5]:
 assert entity['ret'] is False
 assert entity['opt'] is (entity['uniq_id'] in ('haier_test_quiet','haier_test_display'))
 assert entity['dev']['ids']==['haier_test']
 for k,v in entity.items():
  if isinstance(v,str) and '{{' in v: assert e.from_string(v).render(value_json=samples,value='22' if k=='temp_cmd_tpl' else 'auto')
 for av in entity['avty']:
  if 'val_tpl' in av:
   assert e.from_string(av['val_tpl']).render(value_json=samples)=='online'
   assert e.from_string(av['val_tpl']).render(value_json={**samples,'available':False})=='offline'
 assert len(json.dumps(entity,separators=(',',':')).encode())<1800
assert e.from_string(a[0]['mode_stat_tpl']).render(value_json=samples)=='auto'
assert e.from_string(a[0]['temp_cmd_tpl']).render(value=22.0)=='22'
assert len({v['uniq_id'] for v in a})==8

# HA rejects PRESET_NONE in configured presets; it adds the reset option itself.
assert "none" not in a[0]["pr_modes"]
assert a[0]["modes"] == ["off", "cool", "heat", "dry", "fan_only", "auto"]



# JSON null must become HA reset token None, never invalid lowercase mode none.
for key,field in [("mode_stat_tpl","mode"),("fan_mode_stat_tpl","fan"),("swing_mode_stat_tpl","swing")]:
 assert e.from_string(a[0][key]).render(value_json={**samples,field:None})=="None"
 assert e.from_string(a[0][key]).render(value_json=samples) in ("auto","high","both")

print('Discovery JSON, templates, null-state handling and climate rules: PASS')

# Optimistic switches retain authoritative state subscriptions and both values.
for entity,field in [(a[1],"quiet"),(a[2],"display")]:
 assert entity["stat_t"] == "~/state"
 for value,expected in [(True,"ON"),(False,"OFF")]:
  assert e.from_string(entity["val_tpl"]).render(value_json={**samples,field:value}) == expected

assert a[0]["mode_cmd_t"]=="~/set/hvac_mode"
# Longest supported prefix must fit the worker output buffer.
for entity in a:
 wire=json.dumps(entity,separators=(",",":")).replace("haier/test","x"*64).replace("haier_test","haier-s3-abcdef")
 assert len(wire.encode())+128<2048

# Diagnostic freshness and fault use the independent observation stream.
for entity in a[5:]:
 assert entity['stat_t']=='~/telemetry'
 assert entity['exp_aft']==15
 assert entity['dev']['ids']==['haier_test']
assert a[5]['frc_upd'] is True
assert a[5]['json_attr_t']=='~/telemetry'
assert a[6]['dev_cla']=='problem'
for valid in (True,False):
 for code in (0,1,255):
  payload={'fault_valid':valid,'fault':bool(code) if valid else None,'fault_code':code if valid else None,'boot_id':'abc','status_frames':42,'age_ms':100}
  for entity in a[6:]:
   assert e.from_string(entity['avty'][1]['val_tpl']).render(value_json=payload)==('online' if valid else 'offline')
  assert e.from_string(a[6]['val_tpl']).render(value_json=payload)==('ON' if valid and code else 'OFF')
  assert e.from_string(a[7]['val_tpl']).render(value_json=payload)==(str(code) if valid else 'None')
  attrs=json.loads(e.from_string(a[6]['json_attr_tpl']).render(value_json=payload))
  assert attrs['fault_code']==payload['fault_code']
print('Telemetry discovery, expiry, fault validity and codes: PASS')
