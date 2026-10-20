# SPDX-License-Identifier: GPL-2.0-or-later
"""Checks the C++ server's UPnP port forwarding against a synthetic router on loopback.

Scope: this is a fixture written from the public UPnP IGD specification (WANIPConnection:1
SOAP actions and error codes), not a real router; it checks that ctr_online_server.exe sends
well-formed requests and handles the documented answers. The real routers seen so far: the
developer's router answers SSDP only as a ManageableDevice (IGD disabled), see
reports/online/r8_phase10_server_cpp.md.

The synthetic router:
- describes a WANPPPConnection without an external address and a WANIPConnection with
  203.0.113.50 (TEST-NET-3);
- refuses leases other than 0 with error 725 (OnlyPermanentLeasesSupported);
- holds TCP 3074 for another device (error 718, GetSpecificPortMappingEntry -> 192.168.1.77);
- holds UDP 3078 already for this PC (error 718, entry -> this PC: delete and map again).
Run from the project root after build_online_server.ps1:
    python -B tools/online/test_upnp_client.py
"""
import json, os, re, subprocess, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXE = Path(os.environ.get('CTR_TEST_SERVER_EXE', ROOT / 'build/CTRPort-claude/ctr_online_server.exe')).resolve()
failures = 0
calls = []
mappings = {('TCP', '3074'): '192.168.1.77', ('UDP', '3078'): '127.0.0.1'}

DESCRIPTION = '''<?xml version="1.0"?>
<root xmlns="urn:schemas-upnp-org:device-1-0"><device>
<deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>
<deviceList><device><deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType>
<deviceList><device><deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType>
<serviceList>
<service><serviceType>urn:schemas-upnp-org:service:WANPPPConnection:1</serviceType><controlURL>/ctl/PPP</controlURL></service>
<service><serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType><controlURL>/ctl/IPConn</controlURL></service>
</serviceList></device></deviceList></device></deviceList></device></root>'''


def check(condition, what):
    global failures
    print(json.dumps({'scope': 'upnp_client_test', 'check': what, 'ok': bool(condition)}))
    failures += not condition


def soap_fault(code, text):
    return 500, ('<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body><s:Fault>'
                 '<faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail>'
                 '<UPnPError xmlns="urn:schemas-upnp-org:control-1-0"><errorCode>%d</errorCode>'
                 '<errorDescription>%s</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>' % (code, text))


def soap_ok(action, values=''):
    return 200, ('<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body>'
                 '<u:%sResponse xmlns:u="urn:schemas-upnp-org:service:WANIPConnection:1">%s</u:%sResponse></s:Body></s:Envelope>'
                 % (action, values, action))


class Router(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def answer(self, status, body):
        data = body.encode()
        self.send_response(status)
        self.send_header('Content-Type', 'text/xml; charset="utf-8"')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == '/desc.xml':
            self.answer(200, DESCRIPTION)
        else:
            self.answer(404, '')

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get('Content-Length', 0))).decode()
        service, _, action = self.headers.get('SOAPAction', '').strip('"').partition('#')
        args = dict(re.findall(r'<(New\w+)>([^<]*)</\1>', body))
        calls.append({'path': self.path, 'service': service, 'action': action, 'args': args,
                      'well_formed': body.startswith('<?xml') and '<s:Envelope' in body and ('<u:%s ' % action) in body})
        if self.path == '/ctl/PPP':
            return self.answer(*soap_fault(501, 'ActionFailed'))
        if action == 'GetExternalIPAddress':
            return self.answer(*soap_ok(action, '<NewExternalIPAddress>203.0.113.50</NewExternalIPAddress>'))
        key = (args.get('NewProtocol'), args.get('NewExternalPort'))
        if action == 'AddPortMapping':
            if args.get('NewLeaseDuration') != '0':
                return self.answer(*soap_fault(725, 'OnlyPermanentLeasesSupported'))
            if key in mappings and mappings[key] != args.get('NewInternalClient'):
                return self.answer(*soap_fault(718, 'ConflictInMappingEntry'))
            if key in mappings:  # same client: this router also reports a conflict
                return self.answer(*soap_fault(718, 'ConflictInMappingEntry'))
            mappings[key] = args.get('NewInternalClient')
            return self.answer(*soap_ok(action))
        if action == 'GetSpecificPortMappingEntry':
            if key not in mappings:
                return self.answer(*soap_fault(714, 'NoSuchEntryInArray'))
            return self.answer(*soap_ok(action, '<NewInternalPort>%s</NewInternalPort><NewInternalClient>%s</NewInternalClient>'
                                        '<NewEnabled>1</NewEnabled>' % (key[1], mappings[key])))
        if action == 'DeletePortMapping':
            if key not in mappings:
                return self.answer(*soap_fault(714, 'NoSuchEntryInArray'))
            del mappings[key]
            return self.answer(*soap_ok(action))
        return self.answer(*soap_fault(401, 'Invalid Action'))


server = ThreadingHTTPServer(('127.0.0.1', 0), Router)
threading.Thread(target=server.serve_forever, daemon=True).start()
location = 'http://127.0.0.1:%d/desc.xml' % server.server_address[1]
result = subprocess.run([str(EXE), '--upnp-check', '--upnp-location', location], capture_output=True, text=True, timeout=120)
server.shutdown()
output = result.stdout
print(output, file=sys.stderr)

check(all(c['well_formed'] for c in calls), 'every SOAP request is a well-formed envelope with the action element')
check(all(c['service'] in ('urn:schemas-upnp-org:service:WANIPConnection:1', 'urn:schemas-upnp-org:service:WANPPPConnection:1')
          for c in calls), 'SOAPAction names the router service')
adds = [c for c in calls if c['action'] == 'AddPortMapping']
check(adds and adds[0]['args']['NewLeaseDuration'] == '3600' and all(c['args']['NewLeaseDuration'] == '0' for c in adds[1:]),
      'one-hour lease first, permanent lease after error 725')
check(all(c['path'] == '/ctl/IPConn' for c in adds), 'WAN IP connection used (PPP has no external address)')
check(all(c['args']['NewInternalClient'] == '127.0.0.1' and c['args']['NewInternalPort'] == c['args']['NewExternalPort']
          and c['args']['NewRemoteHost'] == '' and c['args']['NewEnabled'] == '1' for c in adds),
      'mapping to this PC, same internal and external port, any remote host')
check({(c['args']['NewProtocol'], c['args']['NewExternalPort']) for c in adds} ==
      {('TCP', '8780'), ('TCP', '3074'), ('UDP', '3174'), ('UDP', '3078'), ('UDP', '3079'), ('UDP', '3080'), ('UDP', '3081'), ('UDP', '3082'), ('UDP', '8788')},
      'the nine host ports requested (with relay and DHT)')
deletes = [(c['args']['NewProtocol'], c['args']['NewExternalPort']) for c in calls if c['action'] == 'DeletePortMapping']
check(deletes.count(('UDP', '3078')) == 2, 'own stale UDP 3078 mapping deleted, mapped again, deleted at close')
check(('TCP', '3074') not in deletes and mappings == {('TCP', '3074'): '192.168.1.77'},
      'other device\'s TCP 3074 left alone; every mapping made was removed at close')
check(result.returncode == 1 and 'upnp-check: 8/9 porte aperte, indirizzo esterno 203.0.113.50' in output,
      'report: 8 of 9 ports, external address')
check("port already forwarded to another device (192.168.1.77)" in output, 'conflict with another device explained')
print(json.dumps({'scope': 'upnp_client_test', 'passed': failures == 0, 'failures': failures}))
sys.exit(1 if failures else 0)
