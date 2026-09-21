"""Opt-in live HTTP controls test. Changes a real cooling appliance, then restores it."""
import urllib.request, urllib.parse, json, time, argparse, getpass
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--url',required=True)
parser.add_argument('--mac',required=True,help='Expected device MAC; prevents testing the wrong controller')
parser.add_argument('--log',type=Path,required=True,help='Private local JSONL output (contains device identity)')
parser.add_argument('--execute',action='store_true',help='Authorize real appliance changes')
args=parser.parse_args()
if not args.execute:parser.error('--execute is required: this test changes a real appliance')
base=args.url.rstrip('/')
password=getpass.getpass('Controller admin password: ')
m=urllib.request.HTTPPasswordMgrWithDefaultRealm();m.add_password(None,base,"admin",password)
op=urllib.request.build_opener(urllib.request.ProxyHandler({}),urllib.request.HTTPBasicAuthHandler(m))
log=args.log.open("a",encoding="utf8",buffering=1)
def record(x):
 x["time"]=time.strftime("%Y-%m-%dT%H:%M:%S");log.write(json.dumps(x)+"\n");print(json.dumps(x),flush=True)
def get(p):return json.loads(op.open(base+p,timeout=5).read())
h=get("/health")
if h["mac"]!=args.mac.upper():raise RuntimeError("Device MAC mismatch; no commands sent")
initial=get("/haier/status")
if not (initial["available"] and initial["power"] and initial["mode"]=="COOL"):
 raise RuntimeError("Test requires a connected appliance already cooling; no commands sent")
record({"initial":initial,"health":h})
token=get("/haier/test-token")["token"];counter=0
extended={"quiet","display","vertical_position","horizontal_position"}
def command(fields,restoring=False):
 global counter
 counter+=1;rid="release-"+str(int(time.time()))+"-"+str(counter)
 data=urllib.parse.urlencode({"token":token,"request_id":rid,**fields}).encode()
 path="/haier/extended" if next(iter(fields)) in extended else "/haier/control"
 try:
  ack=json.loads(op.open(base+path,data,timeout=5).read());end=time.monotonic()+34
  while time.monotonic()<end:
   time.sleep(1);s=get("/haier/status")
   if s["request_id"]==rid and s["command_state"]!="pending":break
  ok=s["request_id"]==rid and s["command_state"]=="confirmed" and s["command_matches"]>=2
  record({"command":fields,"restoring":restoring,"ok":ok,"state":s})
  if not ok:raise RuntimeError("Command not confirmed")
  time.sleep(2)
 except Exception as e:
  record({"command":fields,"error":str(e),"restoring":restoring});raise
failures=[]
try:
 for fields in ([{"target":str(23 if initial["target_temperature"]!=23 else 22)}]+[{"fan":v} for v in ["LOW","MEDIUM","HIGH","AUTO"]]+[{"display":"OFF"},{"display":"ON"},{"quiet":"ON"},{"quiet":"OFF"}]+[{"preset":v} for v in ["BOOST","NONE","SLEEP","NONE"]]+[{"swing":v} for v in ["VERTICAL","HORIZONTAL","BOTH","OFF"]]+[{"vertical_position":v} for v in ["HEALTH_UP","MAX_UP","HEALTH_DOWN","UP","DOWN","CENTER"]]+[{"horizontal_position":v} for v in ["MAX_LEFT","LEFT","RIGHT","MAX_RIGHT","CENTER"]]):command(fields)
 record({"phase":"web_controls_complete"})
finally:
 # Restore even when an appliance declines one of the optional controls.
 for fields in [{"preset":"NONE"},{"quiet":"OFF"},{"mode":initial["mode"],"target":str(int(initial["target_temperature"])),"fan":initial["fan"],"swing":initial["swing"],"preset":initial["preset"]},{"vertical_position":initial["vertical_position"]},{"horizontal_position":initial["horizontal_position"]},{"display":"ON" if initial["display"] else "OFF"},{"quiet":"ON" if initial["quiet"] else "OFF"}]:
  try:command(fields,True)
  except Exception as e:failures.append(str(e))
 final=get("/haier/status")
 keys=["power","mode","target_temperature","fan","swing","preset","quiet","display","vertical_position","horizontal_position"]
 restored=all(final[k]==initial[k] for k in keys)
 record({"final":final,"health":get("/health"),"restored":restored,"restore_errors":failures});log.close()
 if failures or not restored:raise RuntimeError("Restoration failed; inspect the appliance and local log")
