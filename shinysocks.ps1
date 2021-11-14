
Start-Process .\build\RelWithDebInfo\shinysocks.exe -NoNewWindow

Start-Sleep -Seconds 2

Get-Content -Path .\shinysocks_*.log -Tail 300 -Wait
