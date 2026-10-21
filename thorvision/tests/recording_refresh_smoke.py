"""Windows UI refresh/memory regression. Requires a smoke-enabled ThorVision build.

--no-preview uses only an isolated local status fixture. Without it, reads the
real device and attaches a receiver, without changing its recording settings.
"""
import argparse,ctypes,json,os,subprocess,threading,time,urllib.request
from ctypes import wintypes
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
parser=argparse.ArgumentParser();parser.add_argument('executable');parser.add_argument('--name',required=True);parser.add_argument('--seconds',type=int,default=90);parser.add_argument('--no-preview',action='store_true');parser.add_argument('--test-operation',action='store_true');parser.add_argument('--host',default='192.168.50.206');args=parser.parse_args()
if args.test_operation and not args.no_preview:parser.error('Operation test requires the isolated fixture')
class Counters(ctypes.Structure):
 _fields_=[('cb',wintypes.DWORD),('PageFaultCount',wintypes.DWORD)]+[(field,ctypes.c_size_t) for field in ['PeakWorkingSetSize','WorkingSetSize','QuotaPeakPagedPoolUsage','QuotaPagedPoolUsage','QuotaPeakNonPagedPoolUsage','QuotaNonPagedPoolUsage','PagefileUsage','PeakPagefileUsage','PrivateUsage']]
kernel=ctypes.WinDLL('kernel32',use_last_error=True);psapi=ctypes.WinDLL('psapi',use_last_error=True)
kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
psapi.GetProcessMemoryInfo.argtypes=[wintypes.HANDLE,ctypes.POINTER(Counters),wintypes.DWORD]
kernel.CloseHandle.argtypes=[wintypes.HANDLE]
state={'version':1,'revision':1,'observed_at':time.time(),'segment_seconds':60,'busy':False,
 'storage':{'state':'Ready','uuid':'fixture','generation':'fixture','free_bytes':500000000000,'total_bytes':1000000000000,'port_configured':True,'repair_enabled':False},
 'cameras':[{'stable_id':f'fixture-{i}','slot':i,'name':f'Camera {i+1}','enabled':True,'state':'Recording','profile_source':'saved','profile':{'codec':'h265','width':1920,'height':1080,'fps_n':30,'fps_d':1}} for i in range(4)],'events':[]}
operations=[]
class Handler(BaseHTTPRequestHandler):
 def do_GET(self):
  if self.path=='/recording':
   time.sleep(.15);state['observed_at']=time.time();value=state
  else:value=[] if self.path=='/cameras' else {'version':'0.1.2','status':'ok'}
  payload=json.dumps(value).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(payload)
 def do_PUT(self):
  body=json.loads(self.rfile.read(int(self.headers.get('Content-Length','0'))));operations.append(body)
  assert body['revision']==state['revision']
  state['revision']+=1;state['segment_seconds']=body['segment_seconds']
  payload=json.dumps(state).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(payload)
 def log_message(self,*unused):pass
server=None
if args.no_preview:
 server=ThreadingHTTPServer(('127.0.0.1',18804),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
env=dict(os.environ) # Use the caller's matching Qt/GStreamer runtime environment.
env['THORVISION_DEVICE_HOST']='127.0.0.1' if args.no_preview else args.host;env['THORVISION_DEVICE_PORT']='18804' if args.no_preview else '8000';env['THORVISION_UI_SMOKE']='1';env['THORVISION_UI_SOAK_MS']=str(args.seconds*1000)
env['THORVISION_SOFTWARE_PREVIEW']='1'
for key in ['THORVISION_UI_SCREENSHOT','THORVISION_PC_SMOKE_DIR']:env.pop(key,None)
if args.test_operation:env['THORVISION_UI_QUEUE_SMOKE']='1'
else:env.pop('THORVISION_UI_QUEUE_SMOKE',None)
root=Path.cwd()
samples=[]
with (root/(args.name+'.log')).open('w',encoding='utf-8') as log:
 proc=subprocess.Popen([args.executable,'debug'],env=env,stdout=log,stderr=log)
 handle=kernel.OpenProcess(0x410,False,proc.pid);started=time.monotonic()
 try:
  while proc.poll() is None:
   counter=Counters();counter.cb=ctypes.sizeof(counter)
   if psapi.GetProcessMemoryInfo(handle,ctypes.byref(counter),ctypes.sizeof(counter)):
    samples.append({'seconds':round(time.monotonic()-started,1),'private_MB':round(counter.PrivateUsage/1e6,1),'working_MB':round(counter.WorkingSetSize/1e6,1)})
    if counter.PrivateUsage>2500000000:proc.terminate();break
   if time.monotonic()-started>args.seconds+30:proc.terminate();break
   time.sleep(2)
  code=proc.wait(timeout=10)
 finally:kernel.CloseHandle(handle)
if server:server.shutdown()
result={'exit':code,'samples':samples,'operations':len(operations)}
(root/(args.name+'.json')).write_text(json.dumps(result,indent=2))
logs=(root/(args.name+'.log')).read_text(encoding='utf-8',errors='replace')
import re
metrics=[tuple(map(int,m)) for m in re.findall(r'REFRESH_SOAK elapsed=(\d+) rows=(\d+) objects=(\d+) replacements=(\d+) busy_edges=(\d+)',logs)]
assert code==0,logs[-6000:]
assert metrics,'No soak metrics; build with THORVISION_ENABLE_UI_SMOKE=ON'
assert metrics[-1][3]==0,metrics[-1] # Camera delegates must survive status updates.
if args.no_preview:assert metrics[-1][4]==(2 if args.test_operation else 0),metrics[-1]
else:assert metrics[-1][4]<=max(6,args.seconds/20),metrics[-1] # Real storage operations can legitimately change busy state.
steady=[row for row in metrics if row[0]>=min(10000,args.seconds*500)]
assert max(row[2] for row in steady)-min(row[2] for row in steady)<100,metrics[-1]
if args.no_preview:assert metrics[-1][1]==4,metrics[-1]
else:assert 'ASYNC_DONE' in logs,'Camera preview never became ready'
if args.test_operation:assert len(operations)==1 and state['segment_seconds']==61,operations
stable=[s['private_MB'] for s in samples if s['seconds']>=min(30,args.seconds/2)]
assert stable and max(stable)-min(stable[:5])<80,stable # Compare growth against the post-warm-up baseline; shutdown releases must not count as growth.
assert max(s['private_MB'] for s in samples)<600,samples[-1]
result['metrics']=metrics[-1]
assert not re.search(r'ReferenceError|TypeError|Cannot assign|is not defined',logs),logs[-6000:]
(root/(args.name+'.json')).write_text(json.dumps(result,indent=2))
print(json.dumps({'exit':code,'samples':samples[::max(1,len(samples)//8)],'metrics':metrics[-1],'operations':len(operations)},indent=2))
