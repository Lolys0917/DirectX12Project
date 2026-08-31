"""UI smoke checks against only the editor process launched for this verification."""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import json, time, sys
from PIL import Image

u = c.WinDLL('user32', use_last_error=True)
k = c.WinDLL('kernel32', use_last_error=True)
g = c.WinDLL('gdi32', use_last_error=True)
u.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.SendMessageW.restype = w.LPARAM
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetDlgCtrlID.argtypes = [w.HWND]
u.IsWindowVisible.argtypes = [w.HWND]
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.SetWindowTextW.argtypes = [w.HWND, w.LPCWSTR]
callback = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
u.EnumWindows.argtypes = [callback, w.LPARAM]
u.EnumChildWindows.argtypes = [w.HWND, callback, w.LPARAM]
pid = int(Path('Tests/.output/preview-process.txt').read_text(encoding='utf-8-sig').strip())
windows = []
def top(hwnd, _):
    found = w.DWORD(); u.GetWindowThreadProcessId(hwnd, c.byref(found))
    if found.value == pid: windows.append(hwnd)
    return True
def text(hwnd):
    s = c.create_unicode_buffer(32768); u.SendMessageW(hwnd, 0xD, len(s), c.addressof(s)); return s.value
def set_text(hwnd, value):
    s=c.create_unicode_buffer(value);return u.SendMessageW(hwnd,0xC,0,c.addressof(s))
for _ in range(100):
    windows.clear(); u.EnumWindows(callback(top), 0)
    hwnd = next((h for h in windows if 'DirectX 12 Component Engine' in text(h)), None)
    if hwnd: break
    time.sleep(.2)
if not hwnd: raise AssertionError('Editor window not found')
controls = []
for _ in range(100):
    controls.clear();u.EnumChildWindows(hwnd, callback(lambda h, _: controls.append(h) or True), 0)
    ids = {u.GetDlgCtrlID(h): h for h in controls if u.GetDlgCtrlID(h)}
    if 1140 in ids and 1120 in ids and 1008 in ids: break
    time.sleep(.2)
def send(h, msg, wp=0, lp=0): return u.SendMessageW(h, msg, wp, lp)
def tab(index):
    k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
    k.VirtualAllocEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD,w.DWORD];k.VirtualAllocEx.restype=c.c_void_p
    k.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.c_void_p]
    k.VirtualFreeEx.argtypes=[w.HANDLE,c.c_void_p,c.c_size_t,w.DWORD]
    k.CloseHandle.argtypes=[w.HANDLE]
    process=k.OpenProcess(0x38,False,pid);memory=k.VirtualAllocEx(process,None,16,0x3000,4)
    try:
        send(ids[1008],0x130C,index)
        send(ids[1008],0x130C,index-1 if index else 1)
        assert send(ids[1008],0x130A,index,memory),'Tab rectangle unavailable'
        rect=w.RECT();assert k.ReadProcessMemory(process,memory,c.byref(rect),16,None)
        point=((rect.top+rect.bottom)//2<<16)|((rect.left+rect.right)//2)
        send(ids[1008],0x201,1,point);send(ids[1008],0x202,0,point)
    finally:
        k.VirtualFreeEx(process,memory,0,0x8000);k.CloseHandle(process)
    time.sleep(.7)
def click(id): send(ids[id], 0xF5); time.sleep(.3)
def choose(index):
    send(ids[1120], 0x186, index)
    send(hwnd, 0x111, 1120 | (1 << 16), ids[1120])
    for _ in range(150):
        time.sleep(.2)
        if not any('読み込み中' in text(h) for h in controls): break
    else: raise AssertionError('Asset did not finish loading')
    time.sleep(.3)
def entries(id):
    result=[]
    for i in range(send(ids[id], 0x18B)):
        s=c.create_unicode_buffer(32768); send(ids[id],0x189,i,c.addressof(s)); result.append(s.value)
    return result

def capture(name):
    class BI(c.Structure):
        _fields_=[('size',w.DWORD),('width',w.LONG),('height',w.LONG),('planes',w.WORD),('bits',w.WORD),
            ('compression',w.DWORD),('imageSize',w.DWORD),('xppm',w.LONG),('yppm',w.LONG),('used',w.DWORD),('important',w.DWORD)]
    r=w.RECT();u.GetWindowRect(hwnd,c.byref(r));width=r.right-r.left;height=r.bottom-r.top
    g.CreateCompatibleDC.argtypes=[w.HDC];g.CreateCompatibleDC.restype=w.HDC
    g.CreateDIBSection.argtypes=[w.HDC,c.POINTER(BI),w.UINT,c.POINTER(c.c_void_p),w.HANDLE,w.DWORD];g.CreateDIBSection.restype=w.HBITMAP
    g.SelectObject.argtypes=[w.HDC,w.HGDIOBJ];g.SelectObject.restype=w.HGDIOBJ
    g.DeleteObject.argtypes=[w.HGDIOBJ];g.DeleteDC.argtypes=[w.HDC]
    u.PrintWindow.argtypes=[w.HWND,w.HDC,w.UINT]
    dc=g.CreateCompatibleDC(None);bits=c.c_void_p();bi=BI(40,width,-height,1,32,0,0,0,0,0,0)
    bitmap=g.CreateDIBSection(dc,c.byref(bi),0,c.byref(bits),None,0);old=g.SelectObject(dc,bitmap)
    u.PrintWindow(hwnd,dc,2)
    Image.frombuffer('RGBA',(width,height),c.string_at(bits,width*height*4),'raw','BGRA',0,1).convert('RGB').save('Tests/.output/'+name+'.png')
    g.SelectObject(dc,old);g.DeleteObject(bitmap);g.DeleteDC(dc)

if __name__ == '__main__':
    u.ShowWindow.argtypes=[w.HWND,c.c_int]
    u.ShowWindow(hwnd,9)
    time.sleep(1)
    assert send(ids[1008],0x1304)==8,'Expected eight tabs'
    tab(1)
    assert u.IsWindowVisible(ids[1140]),'Playback settings not visible'
    assert not u.IsWindowVisible(ids[1120]),'Asset list leaked into playback tab'
    capture('playback-settings')
    tab(7)
    assert u.IsWindowVisible(ids[1120]),'Preview tab not visible'
    files=entries(1120);print(json.dumps(files,ensure_ascii=False))
    dds=next(i for i,v in enumerate(files) if 'joran-quinten' in v)
    choose(dds);capture('dds-preview')
    obj=next(i for i,v in enumerate(files) if v.startswith('[OBJ]'))
    choose(obj);capture('model-front')
    click(1109);click(1110);capture('model-rotated')
    click(1113);capture('model-zoomed')
    Path('Tests/.output/ui-log.json').write_text(json.dumps(entries(1006),ensure_ascii=False,indent=2),encoding='utf-8')
    records=[]
    for h in controls:
        if not u.IsWindowVisible(h):continue
        r=w.RECT();u.GetWindowRect(h,c.byref(r)); records.append({'id':u.GetDlgCtrlID(h),'text':text(h),'rect':[r.left,r.top,r.right,r.bottom]})
    Path('Tests/.output/ui-controls.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Playback settings, DDS/OBJ selection, rotation and zoom controls exercised.')
