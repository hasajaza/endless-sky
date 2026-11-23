# Removes Endless Sky Accessible. Saved games, preferences and the original game are kept.
param([string]$InstallDir = "")
if(-not $InstallDir) { $InstallDir = Join-Path $env:LOCALAPPDATA "Endless Sky Accessible" }
$links = @(
	(Join-Path ([Environment]::GetFolderPath("Desktop")) "Endless Sky Accessible.lnk"),
	(Join-Path ([Environment]::GetFolderPath("Programs")) "Endless Sky Accessible.lnk")
)
foreach($link in $links) { if(Test-Path $link) { Remove-Item $link -Force } }
if(Test-Path (Join-Path $InstallDir "Endless Sky.exe")) {
	Remove-Item $InstallDir -Recurse -Force
	Write-Host "Endless Sky Accessible was removed. Your saved games were not touched."
} else {
	Write-Host "Nothing to remove in $InstallDir."
}
