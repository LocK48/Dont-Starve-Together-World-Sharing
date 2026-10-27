# Tai PortableGit (Git for Windows) vao thu muc "git" ben canh file nay.
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$dir = Join-Path $PSScriptRoot 'git'
if (Test-Path (Join-Path $dir 'cmd\git.exe')) { Write-Host 'Git da co san.'; exit 0 }

Write-Host 'Dang tim ban PortableGit moi nhat...'
$rel = Invoke-RestMethod 'https://api.github.com/repos/git-for-windows/git/releases/latest' -Headers @{ 'User-Agent' = 'dstsync' }
$asset = $rel.assets | Where-Object { $_.name -match '^PortableGit-.*-64-bit\.7z\.exe$' } | Select-Object -First 1
if (-not $asset) { throw 'Khong tim thay file PortableGit 64-bit trong release.' }

$tmp = Join-Path $env:TEMP $asset.name
Write-Host "Dang tai $($asset.name) ..."
Invoke-WebRequest $asset.browser_download_url -OutFile $tmp -UseBasicParsing

Write-Host 'Dang giai nen...'
New-Item -ItemType Directory -Force -Path $dir | Out-Null
Start-Process -FilePath $tmp -ArgumentList @("-o`"$dir`"", '-y') -Wait
Remove-Item $tmp -Force

if (-not (Test-Path (Join-Path $dir 'cmd\git.exe'))) { throw 'Giai nen xong nhung khong thay git\cmd\git.exe' }
Write-Host 'Hoan tat. Git da san sang.'
