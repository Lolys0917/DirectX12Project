Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class NativeMainSceneVerification
{
    public delegate bool EnumWindowCallback(IntPtr hwnd, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowCallback callback, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowCallback callback, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maximumLength);

    [DllImport("user32.dll")]
    public static extern int GetDlgCtrlID(IntPtr hwnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wparam, StringBuilder lparam);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);

    public static IntPtr FindTopWindow(uint expectedProcessId)
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr lparam)
        {
            uint processId;
            GetWindowThreadProcessId(hwnd, out processId);
            StringBuilder title = new StringBuilder(256);
            GetWindowText(hwnd, title, title.Capacity);

            if (processId == expectedProcessId &&
                title.ToString().Contains("DirectX 12 Component Engine"))
            {
                result = hwnd;
                return false;
            }

            return true;
        }, IntPtr.Zero);
        return result;
    }

    public static IntPtr FindChildById(IntPtr parent, int expectedId)
    {
        IntPtr result = IntPtr.Zero;
        EnumChildWindows(parent, delegate(IntPtr hwnd, IntPtr lparam)
        {
            if (GetDlgCtrlID(hwnd) == expectedId)
            {
                result = hwnd;
                return false;
            }

            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@

$ExecutablePath = Join-Path $PSScriptRoot "x64\Debug\DirectX12Project.exe"
$Process = Start-Process -FilePath $ExecutablePath -WorkingDirectory (Split-Path $ExecutablePath) -WindowStyle Hidden -PassThru
$WindowHandle = [IntPtr]::Zero

try
{
    for ($Attempt = 0; $Attempt -lt 200; ++$Attempt)
    {
        Start-Sleep -Milliseconds 100
        $WindowHandle = [NativeMainSceneVerification]::FindTopWindow([uint32]$Process.Id)

        if ($WindowHandle -ne [IntPtr]::Zero)
        {
            break
        }
    }

    if ($WindowHandle -eq [IntPtr]::Zero)
    {
        throw "Editor window was not created."
    }

    $LogListHandle = [IntPtr]::Zero

    for ($Attempt = 0; $Attempt -lt 200; ++$Attempt)
    {
        Start-Sleep -Milliseconds 100
        $LogListHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1006)
        $LogCount = if ($LogListHandle -eq [IntPtr]::Zero) { 0 } else {
            [NativeMainSceneVerification]::SendMessage($LogListHandle, 0x018B, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
        }

        if ($LogCount -ge 5)
        {
            break
        }
    }

    if ($LogListHandle -eq [IntPtr]::Zero -or $LogCount -lt 5)
    {
        throw "Message log was not initialized."
    }

    $Messages = New-Object System.Collections.Generic.List[string]

    for ($Index = 0; $Index -lt $LogCount; ++$Index)
    {
        $TextLength = [NativeMainSceneVerification]::SendMessage(
            $LogListHandle,
            0x018A,
            [IntPtr]$Index,
            [IntPtr]::Zero
        ).ToInt32()
        $Text = New-Object System.Text.StringBuilder($TextLength + 1)
        [NativeMainSceneVerification]::SendMessage(
            $LogListHandle,
            0x0189,
            [IntPtr]$Index,
            $Text
        ) | Out-Null
        $Messages.Add($Text.ToString())
    }

    $CombinedMessages = $Messages -join "`n"

    if ($CombinedMessages -notmatch "MainScene \| DemoModel definition created" -or
        $CombinedMessages -notmatch "SceneManager \| MainScene changed" -or
        $CombinedMessages -notmatch "GameApp \| SceneManager initialized the active MainScene")
    {
        throw "MainScene lifecycle logs were not found.`n$CombinedMessages"
    }

    $Messages
}
finally
{
    if ($WindowHandle -ne [IntPtr]::Zero)
    {
        [NativeMainSceneVerification]::PostMessage(
            $WindowHandle,
            0x0010,
            [IntPtr]::Zero,
            [IntPtr]::Zero
        ) | Out-Null
    }

    if (-not $Process.WaitForExit(10000))
    {
        $Process.Kill()
        $Process.WaitForExit()
    }
}
