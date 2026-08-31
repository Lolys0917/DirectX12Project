import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import sys,time,json,configparser
import InspectAssetPreview as ui
from PIL import Image

def wait_for(predicate, description, seconds=30):
    start=time.monotonic()
    while time.monotonic()-start<seconds:
        if predicate():return
        time.sleep(.2)
    raise AssertionError(description)

def status_has(value): return any(value in ui.text(h) for h in ui.controls)

def file_dialog(button, path):
    ui.u.PostMessageW.argtypes=ui.u.SendMessageW.argtypes
    ui.u.PostMessageW(ui.ids[button],0xF5,0,0)
    dialogs=[]
    def find():
        dialogs.clear()
        def each(h,_):
            pid=w.DWORD();ui.u.GetWindowThreadProcessId(h,c.byref(pid))
            name=c.create_unicode_buffer(100);ui.u.GetClassNameW(h,name,100)
            if pid.value==ui.pid and name.value=='#32770':dialogs.append(h)
            return True
        ui.u.EnumWindows(ui.callback(each),0)
        return bool(dialogs)
    wait_for(find,'File dialog did not open')
    # A native dialog can become enumerable before WM_INITDIALOG has finished.
    time.sleep(2)
    dialog=dialogs[0];children=[]
    ui.u.EnumChildWindows(dialog,ui.callback(lambda h,_:children.append(h) or True),0)
    fields=[]
    for h in children:
        name=c.create_unicode_buffer(100);ui.u.GetClassNameW(h,name,100)
        if ui.u.GetDlgCtrlID(h)==1148 and name.value=='Edit':fields.insert(0,h)
        elif name.value=='Edit' and ui.u.IsWindowVisible(h):fields.append(h)
    if not fields: raise AssertionError('File-name field not found')
    resolved=str(Path(path).resolve())
    assert Path(resolved).is_file(),resolved
    ui.set_text(fields[0],resolved)
    time.sleep(.5)
    assert ui.text(fields[0])==resolved,'File-name field changed during initialization'
    open_button=next(h for h in children if ui.u.GetDlgCtrlID(h)==1)
    ui.u.PostMessageW(open_button,0xF5,0,0)
    ui.u.IsWindow.argtypes=[w.HWND]
    wait_for(lambda:not ui.u.IsWindow(dialog),'File dialog did not accept the selected file')

ui.u.ShowWindow.argtypes=[w.HWND,c.c_int];ui.u.ShowWindow(ui.hwnd,9)
ui.tab(1)
settings=Path('Assets/EditorSettings.ini')
def read_settings():
    config=configparser.ConfigParser();config.read(settings,encoding='utf-16');return config['Playback']
ui.set_text(ui.ids[1140],'0.5');ui.click(1160)
wait_for(lambda:settings.exists() and read_settings().get('TimeScale')=='0.5','Game speed not saved')
before=settings.read_bytes()
ui.set_text(ui.ids[1141],'not-a-number');ui.click(1160)
assert settings.read_bytes()==before,'Invalid setting changed stored settings'
ui.click(1161)
wait_for(lambda:read_settings().get('TimeScale')=='1','Defaults not restored')
assert ui.text(ui.ids[1141])=='0'
print('PASS settings save, invalid-input rejection and reset',flush=True)

ui.tab(7)
files=ui.entries(1120)
ui.choose(next(i for i,v in enumerate(files) if v.startswith('[OBJ]')))
wait_for(lambda:status_has('OBJ /'),'OBJ did not appear')
ui.capture('model-loaded')
# Z-up source used by the sample cat: orient it with the available arrows.
for _ in range(6):ui.click(1110)
for _ in range(12):ui.click(1109)
ui.capture('model-oriented')
file_dialog(1107,'Assets/Textures/joran-quinten-CRmulUkILVg-unsplash.dds')
wait_for(lambda:status_has('モデルプレビューにテクスチャを適用'),'DDS not applied to preview')
ui.capture('model-dds-texture')
print('PASS OBJ orientation and DDS material preview',flush=True)

# Import a known small image through the actual file dialog.
fixture=Path('Tests/.output/PreviewImportSmoke.png')
Image.new('RGBA',(24,12),(200,80,20,128)).save(fixture)
destination=Path('Assets/Textures/PreviewImportSmoke.png')
assert not destination.exists(),'Test asset would collide'
file_dialog(1100,fixture)
wait_for(lambda:destination.exists(),'File import did not copy the asset')
wait_for(lambda:status_has('画像 / 24 × 12'),'Imported image not previewed')
assert destination.read_bytes()==fixture.read_bytes()
ui.capture('imported-transparent-image')
print('PASS file import, dimensions and alpha preview',flush=True)

# Resize the entire editor to its smallest useful size and exercise both pages.
ui.u.SetWindowPos.argtypes=[w.HWND,w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.UINT]
ui.u.SetWindowPos(ui.hwnd,None,0,0,730,680,0x16)
time.sleep(.5);ui.tab(1);ui.capture('playback-small')
ui.tab(7);ui.capture('preview-small')
ui.u.SetWindowPos(ui.hwnd,None,0,0,1296,759,0x16)
time.sleep(.5)
files=ui.entries(1120)
ui.choose(next(i for i,v in enumerate(files) if 'joran-quinten' in v))
ui.click(1105)
wait_for(lambda:status_has('適用しました'),'Background did not apply')
assert 'joran-quinten' in read_settings().get('SkyTexture','')
ui.capture('final-preview')
print('PASS resize and background selection persistence',flush=True)
Path('Tests/.output/workflow-log.json').write_text(json.dumps(ui.entries(1006),ensure_ascii=False,indent=2),encoding='utf-8')
ui.u.PostMessageW(ui.hwnd,0x10,0,0)
time.sleep(1)
destination.unlink();fixture.unlink()
print('All asset workflow checks passed.',flush=True)
