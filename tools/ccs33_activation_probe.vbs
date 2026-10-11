' license:BSD-3-Clause
' copyright-holders:Gaz
' Optional vendor-tool control. No DSP program is loaded or executed.
Option Explicit
On Error Resume Next
Dim ccs, opened, board, cpu
Set ccs = CreateObject("CCS_Scripting_Com.CCS_Scripting")
Check "CreateObject"
WScript.Echo "version=" & ccs.ScriptGetVersion()
Check "ScriptGetVersion"
ccs.ScriptTimeoutSet 15000
Check "ScriptTimeoutSet"
opened = ccs.CCSOpenNamed("*", "*", 0)
Check "CCSOpenNamed"
WScript.Echo "open=" & opened
board = ccs.TargetGetBoardName()
Check "TargetGetBoardName"
cpu = ccs.TargetGetCPUName()
Check "TargetGetCPUName"
WScript.Echo "board=" & board
WScript.Echo "cpu=" & cpu
ccs.CCSClose 1
Check "CCSClose"
If Len(Trim(CStr(board))) = 0 Or Len(Trim(CStr(cpu))) = 0 Then
    WScript.Echo "CCS33 activation rejected: empty target identity"
    WScript.Quit 1
End If
WScript.Echo "CCS33 activation: PASS target_identity_only=1 pipeline_claim=0"
Sub Check(stage)
    If Err.Number <> 0 Then
        WScript.Echo "CCS33 activation rejected: " & stage & ": " & Err.Number & " " & Err.Description
        WScript.Quit 1
    End If
End Sub
