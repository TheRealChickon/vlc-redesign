"""Local protocol fixture. It never contacts OpenSubtitles or uses real credentials."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
import json

def start_fixture():
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def send(self, body, content_type='application/json'):
            self.send_response(200)
            self.send_header('Content-Type',content_type)
            self.send_header('Content-Length',str(len(body)))
            self.end_headers(); self.wfile.write(body)
        def do_POST(self):
            payload=json.loads(self.rfile.read(int(self.headers.get('Content-Length',0))))
            if self.path.endswith('/login'):
                assert payload=={'username':'local-fixture-user','password':'local-fixture-password'}
                response={'token':'local-only-fixture-token'}
            elif self.path.endswith('/download'):
                assert payload['file_id']==641000
                assert self.headers['Authorization']=='Bearer local-only-fixture-token'
                response={'link':f'http://127.0.0.1:{self.server.server_port}/fixture.srt'}
            else: raise AssertionError(self.path)
            self.send(json.dumps(response).encode())
        def do_GET(self):
            if self.path.startswith('/api/v1/subtitles?'):
                self.send(json.dumps({'data':[{'attributes':{'language':'en','moviehash_match':False,'files':[{'file_id':641000,'file_name':'Reviewed fixture.en.srt'}]}}]}).encode())
            elif self.path=='/fixture.srt':
                self.send(b'1\n00:00:01,000 --> 00:00:03,000\nLocal provider fixture\n', 'application/x-subrip')
            else: raise AssertionError(self.path)
    server=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    Thread(target=server.serve_forever,daemon=True).start()
    return server
