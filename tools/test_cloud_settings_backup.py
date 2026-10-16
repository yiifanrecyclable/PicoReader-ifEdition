# SPDX-License-Identifier: Apache-2.0
"""坚果云设置备份使用临时卡与假钥匙串。 / Backup round trips use a private card and fake Keychain."""
from pathlib import Path
import json,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/simulator'))
from cloud_client import CloudController,CloudError

class FakeKeychain:
    def __init__(self):self.values={}
    def get(self,account):return self.values.get(account)
    def save(self,account,secret):self.values[account]=secret
    def forget(self,account):self.values.pop(account,None)

with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='cloud-settings-test-') as tmp:
    work=Path(tmp);card=work/'card';card.mkdir();build=work/'build/simulator';build.mkdir(parents=True)
    keychain=FakeKeychain();controller=CloudController(private_dir=work/'cloud',keychain_factory=lambda:keychain)
    expected={'folder':'测试/PicoBox','account':'backup-only@example.invalid','password':'test-"\\中文'}
    controller.configure(expected)
    assert controller.backup_export()==expected
    assert expected['password'] not in controller.config_file.read_text()
    assert 'password' not in controller.snapshot()
    (build/'titles.txt').write_text('')
    env={**os.environ,'PICO_SIM_CARD_ROOT':str(card),'PICO_SIM_CLOUD_BACKUP_INPUT':'1'}
    def render(events):
        (work/'ui.json').write_text(json.dumps(controller.snapshot(),ensure_ascii=False))
        args=[str(ROOT/'build/simulator/render'),'settings-11',str(work/'ui.json'),'0','48',
              str(build/'frame.bin'),str(build/'titles.txt'),'0','0,0,0,0,0,0,0','builtin',events,'']
        p=subprocess.run(args,cwd=work,env=env,input=json.dumps(controller.backup_export(),ensure_ascii=False),
                         capture_output=True,text=True,timeout=30)
        assert p.returncode==0,'isolated backup render failed'
        return json.loads(p.stdout)
    saved=render('t,340,602,340,602');assert 'cloud_restore' not in saved
    file=card/'Pico-settings.backup';raw=file.read_bytes()
    assert raw[:8]==b'PICOSETB' and expected['password'].encode() in raw
    controller.configure({**expected,'password':'changed-test-only'})
    result=render('t,340,720,340,720;t,340,720,340,720')
    assert result['cloud_restore']==expected
    controller.backup_restore(result.pop('cloud_restore'))
    assert keychain.get(expected['account'])==expected['password']
    assert 'password' not in controller.snapshot() and expected['password'] not in controller.config_file.read_text()
    revision=controller.snapshot()['revision'];controller.backup_restore(expected)
    assert controller.snapshot()['revision']==revision
    raw=bytearray(raw);at=raw.index(expected['password'].encode());raw[at]^=1;file.write_bytes(raw)
    result=render('t,340,720,340,720;t,340,720,340,720')
    assert 'cloud_restore' not in result
    file.write_bytes(bytes(raw));controller.forget()
    render('t,340,602,340,602')
    controller.configure(expected)
    result=render('t,340,720,340,720;t,340,720,340,720')
    controller.backup_restore(result.pop('cloud_restore'))
    assert not controller.snapshot()['configured'] and not keychain.values
    controller.configure(expected);keychain.values.clear()
    try:controller.backup_export()
    except CloudError:pass
    else:raise AssertionError('missing saved secret must not create an incomplete backup')
print('PASS isolated native backup round trips, corruption rejection, Keychain restore and secret-free snapshots')
