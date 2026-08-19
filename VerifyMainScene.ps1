Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class NativeMainSceneVerification
{
    public delegate bool EnumWindowCallback(IntPtr hwnd, IntPtr lparam);

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeMessageHeader
    {
        public IntPtr Window;
        public UIntPtr ControlId;
        public uint Code;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeRectangle
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowCallback callback, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowCallback callback, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maximumLength);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool SetWindowText(IntPtr hwnd, string text);

    [DllImport("user32.dll")]
    public static extern int GetDlgCtrlID(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
    private static extern IntPtr GetWindowLongPtr(IntPtr hwnd, int index);

    [DllImport("user32.dll")]
    public static extern bool IsWindowEnabled(IntPtr hwnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wparam, StringBuilder lparam);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);

    [DllImport("user32.dll", EntryPoint = "SendMessageW", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessageText(IntPtr hwnd, uint message, IntPtr wparam, string lparam);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);

    [DllImport("user32.dll")]
    private static extern bool GetClientRect(IntPtr hwnd, out NativeRectangle rectangle);

    //Summary: Sends a synchronous tab-selection notification.
    //Arguments: parent=Notification target, tab=Tab window, controlId=Tab identifier.
    //Return: None.
    public static void NotifyTabChanged(IntPtr parent, IntPtr tab, int controlId)
    {
        NativeMessageHeader header = new NativeMessageHeader();
        header.Window = tab;
        header.ControlId = new UIntPtr((uint)controlId);
        header.Code = unchecked((uint)-551);
        IntPtr memory = Marshal.AllocHGlobal(Marshal.SizeOf(header));

        try
        {
            Marshal.StructureToPtr(header, memory, false);
            SendMessage(parent, 0x004E, new IntPtr(controlId), memory);
        }
        finally
        {
            Marshal.FreeHGlobal(memory);
        }
    }

    //Summary: Selects a tab through the control's normal mouse-notification path.
    //Arguments: tab=Tab window, index=Zero-based item index.
    //Return: True when the item rectangle was available.
    public static bool ClickTab(IntPtr tab, int index)
    {
        IntPtr memory = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(NativeRectangle)));

        try
        {
            NativeRectangle rectangle;

            if (SendMessage(tab, 0x130A, new IntPtr(index), memory) != IntPtr.Zero)
            {
                rectangle = (NativeRectangle)Marshal.PtrToStructure(
                    memory,
                    typeof(NativeRectangle)
                );
            }
            else
            {
                NativeRectangle client;

                if (!GetClientRect(tab, out client))
                {
                    return false;
                }

                int count = SendMessage(tab, 0x1304, IntPtr.Zero, IntPtr.Zero).ToInt32();
                int itemWidth = Math.Max(1, (client.Right - client.Left) / Math.Max(1, count));
                rectangle.Left = client.Left + itemWidth * index;
                rectangle.Top = client.Top;
                rectangle.Right = rectangle.Left + itemWidth;
                rectangle.Bottom = Math.Min(client.Bottom, client.Top + 32);
            }
            int x = (rectangle.Left + rectangle.Right) / 2;
            int y = (rectangle.Top + rectangle.Bottom) / 2;
            IntPtr position = new IntPtr((y << 16) | (x & 0xffff));
            SendMessage(tab, 0x0201, new IntPtr(1), position);
            SendMessage(tab, 0x0202, IntPtr.Zero, position);
            return true;
        }
        finally
        {
            Marshal.FreeHGlobal(memory);
        }
    }

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

    //Summary: Reads the control's own WS_VISIBLE style without depending on a hidden parent.
    //Arguments: window=Control to inspect.
    //Return: True when the control itself is marked visible.
    public static bool HasVisibleStyle(IntPtr window)
    {
        return (GetWindowLongPtr(window, -16).ToInt64() & 0x10000000L) != 0;
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

    $EditorTabHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1008)
    $ObjectTreeHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1009)
    $ObjectTreeCount = if ($ObjectTreeHandle -eq [IntPtr]::Zero) { 0 } else {
        [NativeMainSceneVerification]::SendMessage($ObjectTreeHandle, 0x1105, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    }

    if ($EditorTabHandle -eq [IntPtr]::Zero -or
        $ObjectTreeHandle -eq [IntPtr]::Zero -or
        $ObjectTreeCount -lt 7)
    {
        throw "Engine tab or Scene/Object/Component tree was not initialized."
    }

    $EditorTabCount = [NativeMainSceneVerification]::SendMessage(
        $EditorTabHandle,
        0x1304,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ).ToInt32()
    $ProgramEditorHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1032)
    $CompileProgramHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1039)
    $ProgramStatusHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1040)
    $ProgramSuggestionHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1041)
    $ProgramRoleHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1042)
    $RestoreProgramHandle = [NativeMainSceneVerification]::FindChildById($WindowHandle, 1043)

    if ($EditorTabCount -ne 5 -or
        $ProgramEditorHandle -eq [IntPtr]::Zero -or
        $CompileProgramHandle -eq [IntPtr]::Zero -or
        $ProgramStatusHandle -eq [IntPtr]::Zero -or
        $ProgramSuggestionHandle -eq [IntPtr]::Zero -or
        $ProgramRoleHandle -eq [IntPtr]::Zero -or
        $RestoreProgramHandle -eq [IntPtr]::Zero)
    {
        throw "Main or Script tab controls were not initialized."
    }

    if ([NativeMainSceneVerification]::HasVisibleStyle($ProgramEditorHandle))
    {
        throw "Program editor was visible over the Engine tab during startup."
    }

    [NativeMainSceneVerification]::PostMessage(
        $WindowHandle,
        0x0111,
        [IntPtr]1039,
        $CompileProgramHandle
    ) | Out-Null
    $MainCompileDetected = $false
    $CapsuleCreated = $false

    for ($Attempt = 0; $Attempt -lt 600; ++$Attempt)
    {
        Start-Sleep -Milliseconds 100
        $LogCount = [NativeMainSceneVerification]::SendMessage(
            $LogListHandle,
            0x018B,
            [IntPtr]::Zero,
            [IntPtr]::Zero
        ).ToInt32()

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

            if ($Text.ToString() -match "MainProgram \| Main extension compiled")
            {
                $MainCompileDetected = $true
            }

            if ($Text.ToString() -match "Object handles and stress-test array initialized")
            {
                $CapsuleCreated = $true
            }
        }

        if ($MainCompileDetected -and $CapsuleCreated)
        {
            break
        }
    }

    if (-not $MainCompileDetected -or -not $CapsuleCreated)
    {
        throw "Threaded Main compile or Capsule creation did not complete. Main=$MainCompileDetected Capsule=$CapsuleCreated"
    }

    [NativeMainSceneVerification]::SendMessage(
        $EditorTabHandle,
        0x130C,
        [IntPtr]3,
        [IntPtr]::Zero
    ) | Out-Null
    [NativeMainSceneVerification]::SendMessage(
        $EditorTabHandle,
        0x0100,
        [IntPtr]0x27,
        [IntPtr]::Zero
    ) | Out-Null
    [NativeMainSceneVerification]::SendMessage(
        $EditorTabHandle,
        0x0101,
        [IntPtr]0x27,
        [IntPtr]::Zero
    ) | Out-Null
    $SelectedTab = [NativeMainSceneVerification]::SendMessage(
        $EditorTabHandle,
        0x130B,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ).ToInt32()
    Start-Sleep -Milliseconds 200

    $RoleText = New-Object System.Text.StringBuilder(512)
    [NativeMainSceneVerification]::GetWindowText(
        $ProgramRoleHandle,
        $RoleText,
        $RoleText.Capacity
    ) | Out-Null

    if ($RoleText.ToString() -notmatch "Unity")
    {
        throw "Script role description was not shown. Selected=$SelectedTab Text=$($RoleText.ToString())"
    }

    [NativeMainSceneVerification]::PostMessage(
        $WindowHandle,
        0x0111,
        [IntPtr]1039,
        $CompileProgramHandle
    ) | Out-Null

    $ManualCompileResult = ""
    $HotReloadDetected = $false

    for ($Attempt = 0; $Attempt -lt 600; ++$Attempt)
    {
        Start-Sleep -Milliseconds 100
        $LogCount = [NativeMainSceneVerification]::SendMessage(
            $LogListHandle,
            0x018B,
            [IntPtr]::Zero,
            [IntPtr]::Zero
        ).ToInt32()

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

            if ($Text.ToString() -match "ScriptProgram \| External Script module compiled")
            {
                $ManualCompileResult = $Text.ToString()
            }

            if ($Text.ToString() -match "ScriptModule \| External Script module hot reloaded")
            {
                $HotReloadDetected = $true
            }
        }

        if ($ManualCompileResult -ne "" -and $HotReloadDetected)
        {
            break
        }
    }

    if ($ManualCompileResult -notmatch "compiled" -or -not $HotReloadDetected)
    {
        throw "Manual Script compile or same-name hot reload did not succeed. Result=$ManualCompileResult HotReload=$HotReloadDetected"
    }

    Start-Sleep -Milliseconds 300
    $AttachedTreeCount = [NativeMainSceneVerification]::SendMessage(
        $ObjectTreeHandle,
        0x1105,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ).ToInt32()

    if ($AttachedTreeCount -lt $ObjectTreeCount + 2)
    {
        throw "Capsule creation or Box Sub Script attachment was not reflected in the Object tree. Before=$ObjectTreeCount After=$AttachedTreeCount"
    }

    if (-not [NativeMainSceneVerification]::IsWindowEnabled($RestoreProgramHandle))
    {
        throw "Last successful source restore was not enabled after compilation."
    }

    $LastGoodSource = Join-Path $PSScriptRoot "ScriptPrograms\.lastgood\BoxKeyboardColorScript.cpp"

    if (-not (Test-Path $LastGoodSource))
    {
        throw "Last successful source snapshot was not captured."
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
        $CombinedMessages -notmatch "GameApp \| SceneManager initialized the active MainScene" -or
        $CombinedMessages -notmatch "MainProgram \| Main extension compiled" -or
        $CombinedMessages -notmatch "Object handles and stress-test array initialized" -or
        $CombinedMessages -notmatch "ScriptProgram \| External Script module compiled" -or
        $CombinedMessages -notmatch "ScriptModule \| External Script module hot reloaded")
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
