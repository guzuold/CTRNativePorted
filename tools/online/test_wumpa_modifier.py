# SPDX-License-Identifier: GPL-2.0-only
"""Loopback integration check: per-session Wumpa modifiers, synthetic catalogue/events."""
import base64,json,socket,struct,subprocess,tempfile,time
from pathlib import Path
import e2e_server_check as wire
from demonware_rest import varint,fields


def blob(tag,value):
    return varint(tag*8+2)+varint(len(value))+value


def main():
    root=Path(tempfile.mkdtemp(prefix='wumpa-modifier-',dir=wire.ROOT/'runtime/user'))
    state=root/'runtime/user/online';state.mkdir(parents=True)
    (state/'retail_reward_catalog.json').write_text(json.dumps({'schema':1,'eboot_sha256':'39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5','tracks':[{'id':101,'ranks':[40,30,20,10]}]}))
    clients=[]
    with (root/'server.log').open('wb') as log:
        process=subprocess.Popen(wire.SERVERS['cpp']+wire.ARGS+['--root',str(root)],cwd=wire.ROOT,stdout=log,stderr=log,creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            for _ in range(80):
                if process.poll() is not None:raise RuntimeError('Test server stopped')
                try:
                    socket.create_connection((wire.HOST,wire.HTTP),timeout=.2).close();break
                except OSError:time.sleep(.1)
            else:raise RuntimeError('Test server not ready')
            def login(name,multiplier):
                headers={'X-CTR-Online-ID':name,'X-CTR-Room-Token':wire.room.token('modifier-check')}
                if multiplier is not None:headers['X-CTR-Wumpa-Multiplier']=multiplier
                request={'auth_task':'46','iv_seed':'1588948653','title_id':'5760','identity':'x','extra_data':'{}'}
                return wire.http('POST','/auth/','ctr-ps4-auth3.prod.demonware.net',headers,json.dumps(request).encode())
            assert login('BadFactor','99')[0]==400
            results=[]
            for name,multiplier,expected in [('NormalFactor',None,40),('BoostFactor','100',4000),('ExplicitNormal','1',40)]:
                status,_,_,body=login(name,multiplier);assert status==200
                client=wire.Lobby(base64.b64decode(json.loads(body)['server_ticket'])[:24]);clients.append(client)
                event=blob(1,b'end_race')+varint(16)+varint(1000)
                for key,value in {'Track':101,'Rank':1,'RaceMode':9,'CompletedLap':0,'Time':120,'Online':0,'RewardGoldenWumpa':0}.items():
                    event+=blob(3,blob(1,key.encode())+varint(16)+varint(value))
                message=blob(1,b'octane_psn')+blob(2,event)
                params=bytes([23,8])+struct.pack('<I',len(message))+message+b'\0'
                for _ in range(2):
                    client.send_task(125,1,params);kind,payload=client.receive()
                    assert kind==1 and payload[9:].hex()=='080000000003010800000000'
                    client.send_task(80,245,bytes.fromhex('17080e0000000a0a6f6374616e655f70736e100300'))
                    kind,payload=client.receive();body=payload[9:]
                    balances=[dict((tag,value) for tag,_,value in fields(entry)) for tag,_,entry in fields(body[13:-1]) if tag==1]
                    assert balances==[{1:10,2:expected*2}],balances
                results.append({'player':name,'multiplier':multiplier or 'default','balance':expected,'duplicate_unchanged':True})
            print(json.dumps({'passed':True,'results':results,'evidence':str(root)}))
        finally:
            for client in clients:client.close()
            process.terminate();process.wait(10)

if __name__=='__main__':main()
