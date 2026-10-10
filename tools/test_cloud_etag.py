# SPDX-License-Identifier: Apache-2.0
"""ETag interoperability over isolated cache/card directories and loopback HTTP.
兼容性检查只用临时目录/虚拟凭据，不读取真实账号，也不回滚用户TF卡。
"""
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from unittest.mock import patch
from xml.sax.saxutils import escape
import hashlib,json,sys,tempfile,threading,unittest,urllib.parse
sys.path.insert(0,str(Path(__file__).resolve().parent/'simulator'))
import cloud_client as cloud

class Bridge:
    def __init__(self):self.native=cloud.NativeBridge();self.commits=[];self.aborts=0
    def call(self,command,*args):
        if command=='abort':self.aborts+=1;return {'error':0}
        return self.native.call(command,*args)
    def require(self,command,*args):
        if command=='prepare':(cloud.STAGE/'attachments').mkdir(parents=True,exist_ok=True);return {'error':0}
        if command=='links':return {'error':0,'names':[]}
        if command=='commit':self.commits.append((args[0],json.loads(Path(args[1]).read_text())));return {'error':0}
        return self.native.require(command,*args)

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_PROPFIND(self):
        self.rfile.read(int(self.headers.get('Content-Length',0)))
        data=('<d:multistatus xmlns:d="DAV:"><d:response><d:href>/dav/PicoBox/fixture.md</d:href><d:propstat><d:prop>'
              '<d:resourcetype/><d:getetag>'+escape(self.server.list_tag)+'</d:getetag><d:getcontentlength>'+str(len(self.server.body))+'</d:getcontentlength>'
              '<d:getlastmodified>Tue, 06 Oct 2026 12:00:00 GMT</d:getlastmodified></d:prop><d:status>HTTP/1.1 200 OK</d:status>'
              '</d:propstat></d:response></d:multistatus>').encode()
        self.send_response(207);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
    def do_GET(self):
        self.server.conditionals.append(self.headers.get('If-Match'))
        if self.headers.get('If-Match') not in (None,self.server.get_tag):self.send_error(412);return
        number=len(self.server.conditionals);body=self.server.second if number>1 and self.server.second is not None else self.server.body
        self.send_response(200);self.send_header('ETag',self.server.get_tag);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)

class EtagTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=cloud.BUILD,prefix='etag-isolated-');root=Path(self.temp.name)
        self.patches=[patch.object(cloud,'CARD',root/'card'),patch.object(cloud,'STAGE',root/'card/.pico-sync/current'),patch.object(cloud,'PRIVATE',root/'private')]
        for p in self.patches:p.start()
        cloud.CARD.mkdir();self.bridge=Bridge();self.http=ThreadingHTTPServer(('127.0.0.1',0),Handler)
        self.http.list_tag='W0FixtureVersionToken_1';self.http.get_tag=self.http.list_tag;self.http.body=b'# one\n';self.http.second=None;self.http.conditionals=[]
        self.thread=threading.Thread(target=self.http.serve_forever,daemon=True);self.thread.start()
        self.client=cloud.DavClient({'folder':'PicoBox','account':'fixture@example.invalid'},'synthetic-password',bridge=self.bridge,test_base=f'http://127.0.0.1:{self.http.server_port}/dav/PicoBox/')
    def tearDown(self):
        self.client.close();self.http.shutdown();self.http.server_close();self.thread.join()
        for p in reversed(self.patches):p.stop()
        self.temp.cleanup()
    def test_bare_nutstore_tag_is_selectable_and_verified_twice(self):
        item=self.client.listing()[0];self.assertTrue(item['selectable']);self.assertEqual(item['error'],'')
        self.client.download_one(item);self.assertEqual(self.http.conditionals,[None,None]);self.assertEqual(len(self.bridge.commits),1)
        self.assertEqual(self.bridge.commits[0][1]['body']['sha256'],hashlib.sha256(self.http.body).hexdigest());self.assertFalse((self.client.cache/'verify-body.part').exists())
    def test_quoted_standard_tag_keeps_one_conditional_download(self):
        self.http.list_tag=self.http.get_tag='"standard-tag"';self.client.download_one(self.client.listing()[0]);self.assertEqual(self.http.conditionals,['"standard-tag"']);self.assertEqual(len(self.bridge.commits),1)
    def test_bare_dav_and_quoted_get_are_same_version(self):
        self.http.get_tag='"'+self.http.list_tag+'"';self.client.download_one(self.client.listing()[0]);self.assertEqual(len(self.bridge.commits),1);self.assertEqual(len(self.http.conditionals),2)
    def test_changed_tag_is_rejected_before_commit(self):
        item=self.client.listing()[0];self.http.get_tag='new-version'
        with self.assertRaises(cloud.CloudError):self.client.download_one(item)
        self.assertEqual(self.bridge.commits,[]);self.assertEqual(self.bridge.aborts,1)
    def test_changed_bytes_even_with_stale_tag_never_commit(self):
        self.http.second=b'# two\n';item=self.client.listing()[0]
        with self.assertRaises(cloud.CloudError):self.client.download_one(item)
        self.assertEqual(self.bridge.commits,[]);self.assertEqual(self.bridge.aborts,1);self.assertFalse((self.client.cache/'verify-body.part').exists())
    def test_without_strong_validator_hash_verifies_before_commit(self):
        for tag in ('','W/"weak"','bad tag'):
            self.http.list_tag=self.http.get_tag=tag;self.http.conditionals=[]
            item=self.client.listing()[0];self.assertTrue(item['selectable'])
            self.client.download_one(item);self.assertEqual(self.http.conditionals,[None,None])
    def test_bare_listing_without_get_tag_uses_two_hash_checked_reads(self):
        self.http.get_tag='';self.client.download_one(self.client.listing()[0])
        self.assertEqual(self.http.conditionals,[None,None]);self.assertEqual(len(self.bridge.commits),1)
    def test_host_and_firmware_parser_agree_on_token_boundaries(self):
        for tag,expected in [('opaque','opaque'),('"opaque"','opaque'),('W0FixtureVersionToken_1','W0FixtureVersionToken_1'),('W/"weak"',None),('""',None),('bad tag',None),('bad\r\ntag',None),('bad"tag',None),('"bad',None),('a'*129,None)]:
            with self.subTest(tag=repr(tag)):
                self.assertEqual(cloud.etag_version(tag),expected);native=self.bridge.native.require('etag',tag);self.assertEqual(native['valid'],expected is not None)
                if expected is not None:self.assertEqual(native['version'],expected)
if __name__=='__main__':unittest.main()
