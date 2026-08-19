Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class NativeProgramEditorLayoutVerification
{
    public delegate bool EnumWindowCallback(IntPtr hwnd, IntPtr lparam);

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeMessageHeader
    {
        public IntPtr Window;
        public UIntPtr ControlId;
        public uint Code;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowCallback callback, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern bool EnumChildWindows(IntPtr parent, EnumWindowCallback callback, IntPtr lparam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int maximumLength);

    [DllImport("user32.dll")]
    public static extern int GetWindowTextLength(IntPtr hwnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool SetWindowText(IntPtr hwnd, string text);

    [DllImport("user32.dll")]
    public static extern int GetDlgCtrlID(IntPtr hwnd);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hwnd);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")]
    private static extern IntPtr GetWindowLongPtr(IntPtr hwnd, int index);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);

    [DllImport("user32.dll", EntryPoint = "SendMessageW", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessageText(IntPtr hwnd, uint message, IntPtr wparam, string lparam);

    [DllImport("user32.dll", EntryPoint = "SendMessageW", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessageBuilder(IntPtr hwnd, uint message, IntPtr wparam, StringBuilder lparam);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);

    //Summary: Finds the Engine top-level window owned by a process.
    //Arguments: expectedProcessId=Process identifier to inspect.
    //Return: Engine window handle, or zero while it is unavailable.
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

    //Summary: Finds a child window by its control identifier.
    //Arguments: parent=Parent window, expectedId=Control identifier to find.
    //Return: Matching child handle, or zero when it is unavailable.
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

    //Summary: Reads all UTF-16 text from a window control.
    //Arguments: window=Control to inspect.
    //Return: Current control text.
    public static string ReadText(IntPtr window)
    {
        int length = GetWindowTextLength(window);
        StringBuilder text = new StringBuilder(length + 1);
        GetWindowText(window, text, text.Capacity);
        return text.ToString();
    }

    //Summary: Reads the client Y coordinate of every logical RichEdit line.
    //Arguments: editor=Program RichEdit control.
    //Return: Client Y coordinates in logical-line order.
    public static int[] ReadLineYCoordinates(IntPtr editor)
    {
        int count = SendMessage(editor, 0x00BA, IntPtr.Zero, IntPtr.Zero).ToInt32();
        List<int> positions = new List<int>();

        for (int line = 0; line < count; ++line)
        {
            int character = SendMessage(editor, 0x00BB, new IntPtr(line), IntPtr.Zero).ToInt32();
            long packed = SendMessage(editor, 0x00D6, new IntPtr(character), IntPtr.Zero).ToInt64();
            positions.Add(unchecked((short)((packed >> 16) & 0xffff)));
        }

        return positions.ToArray();
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
$OriginalText = $null
$ProgramEditorHandle = [IntPtr]::Zero
$ProgramSuggestionHandle = [IntPtr]::Zero
$ProgramStatusHandle = [IntPtr]::Zero

try
{
    for ($Attempt = 0; $Attempt -lt 200; ++$Attempt)
    {
        Start-Sleep -Milliseconds 50
        $WindowHandle = [NativeProgramEditorLayoutVerification]::FindTopWindow([uint32]$Process.Id)

        if ($WindowHandle -ne [IntPtr]::Zero)
        {
            break
        }
    }

    if ($WindowHandle -eq [IntPtr]::Zero)
    {
        throw "Editor window was not created."
    }

    $EditorTabHandle = [IntPtr]::Zero

    for ($Attempt = 0; $Attempt -lt 200; ++$Attempt)
    {
        Start-Sleep -Milliseconds 50
        $EditorTabHandle = [NativeProgramEditorLayoutVerification]::FindChildById(
            $WindowHandle,
            1008
        )
        $ProgramEditorHandle = [NativeProgramEditorLayoutVerification]::FindChildById(
            $WindowHandle,
            1032
        )
        $ProgramSuggestionHandle = [NativeProgramEditorLayoutVerification]::FindChildById(
            $WindowHandle,
            1041
        )
        $ProgramStatusHandle = [NativeProgramEditorLayoutVerification]::FindChildById(
            $WindowHandle,
            1040
        )

        if ($EditorTabHandle -ne [IntPtr]::Zero -and
            $ProgramEditorHandle -ne [IntPtr]::Zero -and
            $ProgramSuggestionHandle -ne [IntPtr]::Zero -and
            $ProgramStatusHandle -ne [IntPtr]::Zero)
        {
            break
        }
    }

    if ($EditorTabHandle -eq [IntPtr]::Zero -or
        $ProgramEditorHandle -eq [IntPtr]::Zero -or
        $ProgramSuggestionHandle -eq [IntPtr]::Zero -or
        $ProgramStatusHandle -eq [IntPtr]::Zero)
    {
        throw "Program editor controls were not created."
    }

    if ([NativeProgramEditorLayoutVerification]::HasVisibleStyle($ProgramEditorHandle))
    {
        throw "Program editor was visible over the Engine tab during startup."
    }

    [NativeProgramEditorLayoutVerification]::SendMessage(
        $EditorTabHandle,
        0x130C,
        [IntPtr]3,
        [IntPtr]::Zero
    ) | Out-Null
    [NativeProgramEditorLayoutVerification]::SendMessage(
        $EditorTabHandle,
        0x0100,
        [IntPtr]0x27,
        [IntPtr]::Zero
    ) | Out-Null
    [NativeProgramEditorLayoutVerification]::SendMessage(
        $EditorTabHandle,
        0x0101,
        [IntPtr]0x27,
        [IntPtr]::Zero
    ) | Out-Null
    Start-Sleep -Milliseconds 200

    if (-not [NativeProgramEditorLayoutVerification]::HasVisibleStyle($ProgramEditorHandle))
    {
        throw "Program editor was not shown after selecting a Program tab."
    }

    $OriginalText = [NativeProgramEditorLayoutVerification]::ReadText($ProgramEditorHandle)
    $BeforeLinePositions = [NativeProgramEditorLayoutVerification]::ReadLineYCoordinates(
        $ProgramEditorHandle
    )
    $EndPosition = [NativeProgramEditorLayoutVerification]::SendMessage(
        $ProgramEditorHandle,
        0x000E,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ).ToInt32()
    [NativeProgramEditorLayoutVerification]::SendMessage(
        $ProgramEditorHandle,
        0x00B1,
        [IntPtr]$EndPosition,
        [IntPtr]$EndPosition
    ) | Out-Null
    [NativeProgramEditorLayoutVerification]::SendMessageText(
        $ProgramEditorHandle,
        0x00C2,
        [IntPtr]1,
        "`r`n"
    ) | Out-Null
    Start-Sleep -Milliseconds 300

    $LinePositions = [NativeProgramEditorLayoutVerification]::ReadLineYCoordinates($ProgramEditorHandle)

    if ($LinePositions.Count -ne $BeforeLinePositions.Count + 1)
    {
        throw "Inserted newline was not reflected in RichEdit line layout. Before=$($BeforeLinePositions.Count) After=$($LinePositions.Count)"
    }

    $LineSteps = for ($Index = 1; $Index -lt $LinePositions.Count; ++$Index)
    {
        $LinePositions[$Index] - $LinePositions[$Index - 1]
    }

    if (($LineSteps | Where-Object { $_ -lt 8 -or $_ -gt 40 }).Count -ne 0)
    {
        throw "RichEdit line spacing is visually inconsistent. Y=$($LinePositions -join ',')"
    }

    [NativeProgramEditorLayoutVerification]::SendMessage(
        $ProgramEditorHandle,
        0x0304,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ) | Out-Null
    Start-Sleep -Milliseconds 200
    $UndoLinePositions = [NativeProgramEditorLayoutVerification]::ReadLineYCoordinates(
        $ProgramEditorHandle
    )

    if ($UndoLinePositions.Count -ne $BeforeLinePositions.Count)
    {
        throw "Undo did not restore the line layout."
    }

    [NativeProgramEditorLayoutVerification]::SendMessage(
        $ProgramEditorHandle,
        0x0454,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ) | Out-Null
    Start-Sleep -Milliseconds 200
    $RedoLinePositions = [NativeProgramEditorLayoutVerification]::ReadLineYCoordinates(
        $ProgramEditorHandle
    )

    if ($RedoLinePositions.Count -ne $LinePositions.Count)
    {
        throw "Redo did not reapply the line layout."
    }

    [NativeProgramEditorLayoutVerification]::SendMessageText(
        $ProgramEditorHandle,
        0x00C2,
        [IntPtr]1,
        "mul"
    ) | Out-Null
    [NativeProgramEditorLayoutVerification]::SendMessage(
        $ProgramEditorHandle,
        0x0101,
        [IntPtr]0x23,
        [IntPtr]::Zero
    ) | Out-Null
    Start-Sleep -Milliseconds 150

    $SuggestionCount = [NativeProgramEditorLayoutVerification]::SendMessage(
        $ProgramSuggestionHandle,
        0x018B,
        [IntPtr]::Zero,
        [IntPtr]::Zero
    ).ToInt32()
    $SuggestionNames = New-Object System.Collections.Generic.List[string]

    for ($Index = 0; $Index -lt $SuggestionCount; ++$Index)
    {
        $TextLength = [NativeProgramEditorLayoutVerification]::SendMessage(
            $ProgramSuggestionHandle,
            0x018A,
            [IntPtr]$Index,
            [IntPtr]::Zero
        ).ToInt32()
        $Text = New-Object System.Text.StringBuilder($TextLength + 1)
        [NativeProgramEditorLayoutVerification]::SendMessageBuilder(
            $ProgramSuggestionHandle,
            0x0189,
            [IntPtr]$Index,
            $Text
        ) | Out-Null
        $SuggestionNames.Add($Text.ToString())
    }

    if ($SuggestionNames -notcontains "MultiplyObjectColor")
    {
        $CurrentEditorText = [NativeProgramEditorLayoutVerification]::ReadText($ProgramEditorHandle)
        $TextTail = $CurrentEditorText.Substring([Math]::Max(0, $CurrentEditorText.Length - 40))
        $SelectedTab = [NativeProgramEditorLayoutVerification]::SendMessage(
            $EditorTabHandle,
            0x130B,
            [IntPtr]::Zero,
            [IntPtr]::Zero
        ).ToInt32()
        $StatusText = [NativeProgramEditorLayoutVerification]::ReadText($ProgramStatusHandle)
        throw "Engine API suggestion was not shown. Tab=$SelectedTab Status=$StatusText Tail=$TextTail Suggestions=$($SuggestionNames -join ',')"
    }

    "Program editor visibility, deferred layout, undo/redo and API suggestions verified. Suggestions=$($SuggestionNames -join ',')"
}
finally
{
    if ($ProgramEditorHandle -ne [IntPtr]::Zero -and $null -ne $OriginalText)
    {
        [NativeProgramEditorLayoutVerification]::SetWindowText(
            $ProgramEditorHandle,
            $OriginalText
        ) | Out-Null
    }

    if ($WindowHandle -ne [IntPtr]::Zero)
    {
        [NativeProgramEditorLayoutVerification]::PostMessage(
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
