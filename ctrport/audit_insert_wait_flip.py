# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 p=Path('reports/reconstruction/infra/shareplay_pending_startup.json');r=json.loads(p.read_text())
 calls=[e for e in r['events'] if e.get('scope')=='original_GnmInsertWaitFlipDone_return'];assert calls
 assert all(e['result']==0 and e['size']==7 and e['slot']<16 and e['real_label_address']>0 and not e['GPU_wait_executed'] for e in calls)
 control=json.loads(Path('runtime/user/logs/infra_insert_wait_flip_control.log').read_text());assert control['passed'] and control['negative_rejections']==3
 assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
 result=dict(scope='retail_InsertWaitFlipDone_packet_emission',passed=True,timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),calls=calls,authored_control=control,source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),exe_sha256=r['exe_sha256'],R3=False,engine_replacements=0,limitation='Packet writes real slot label address; emitting a wait command does not prove subsequent GPU wait execution. No synthetic completion.')
 Path('reports/reconstruction/infra/insert_wait_flip_retail.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
