param([int]$GamePid, [string]$OutputPath)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class MenuCapture {
 public delegate bool EnumProc(IntPtr hwnd,IntPtr data);
 [DllImport("user32.dll")]public static extern bool EnumWindows(EnumProc callback,IntPtr data);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int count);
 [DllImport("user32.dll")]public static extern bool GetClientRect(IntPtr hwnd,out Rect rect);
 [DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr hwnd,IntPtr hdc,uint flags);
 public struct Rect {public int left,top,right,bottom;}
}
'@
$script:gameWindow=[IntPtr]::Zero
[MenuCapture]::EnumWindows({param($handle,$data)
    [uint32]$ownerPid=0
    [void][MenuCapture]::GetWindowThreadProcessId($handle,[ref]$ownerPid)
    if($ownerPid -eq $GamePid){
        $title=New-Object System.Text.StringBuilder 512
        [void][MenuCapture]::GetWindowText($handle,$title,512)
        if($title.ToString().StartsWith('Counter-Strike: Global Offensive')){$script:gameWindow=$handle}
    }
    return $true
},[IntPtr]::Zero) | Out-Null
if($script:gameWindow -eq [IntPtr]::Zero){throw 'Game window not found'}
$rect=New-Object MenuCapture+Rect
[void][MenuCapture]::GetClientRect($script:gameWindow,[ref]$rect)
$bitmap=New-Object System.Drawing.Bitmap ($rect.right-$rect.left),($rect.bottom-$rect.top)
$graphics=[System.Drawing.Graphics]::FromImage($bitmap)
$dc=$graphics.GetHdc()
try { $ok=[MenuCapture]::PrintWindow($script:gameWindow,$dc,3) }
finally { $graphics.ReleaseHdc($dc);$graphics.Dispose() }
try {$bitmap.Save($OutputPath,[System.Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
Write-Output "PrintWindow=$ok window=$script:gameWindow output=$OutputPath"
