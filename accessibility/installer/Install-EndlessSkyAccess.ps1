# Endless Sky Accessible - installer
# Installs the accessible game build into its own folder and points it at the
# data of your existing Endless Sky 0.11.4 installation: the Steam version or
# the official download from GitHub (EndlessSky-win64-v0.11.4.zip, extracted).
# Nothing in the original installation is changed. Saved games are shared.
param(
	[string]$GamePath = "",
	[string]$InstallDir = "",
	[switch]$Quiet
)
$ErrorActionPreference = "Stop"

function Say([string]$text) { Write-Host $text }

function Ask([string]$question) {
	if($Quiet) { return "" }
	return Read-Host $question
}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $here "game"
if(-not (Test-Path (Join-Path $source "Endless Sky.exe"))) {
	Say "Error: the folder named game was not found next to this script. Extract the whole zip file first."
	exit 1
}

function Test-GameFolder([string]$path) {
	if(-not $path) { return $false }
	return (Test-Path (Join-Path $path "credits.txt")) -and (Test-Path (Join-Path $path "data"))
}

# Folders of the official GitHub download: look next to and above the folder
# this installer was extracted to, then in Downloads, Desktop and Documents.
function Find-DownloadedGame {
	$places = New-Object System.Collections.Generic.List[string]
	$parent = Split-Path -Parent $here
	if($parent) {
		$places.Add($parent)
		$grand = Split-Path -Parent $parent
		if($grand) { $places.Add($grand) }
	}
	foreach($root in @($parent, (Join-Path $env:USERPROFILE "Downloads"),
			[Environment]::GetFolderPath("Desktop"), [Environment]::GetFolderPath("MyDocuments"),
			"C:\Games", "D:\Games")) {
		if(-not $root -or -not (Test-Path $root)) { continue }
		foreach($dir in (Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue)) {
			$places.Add($dir.FullName)
			foreach($sub in (Get-ChildItem -Path $dir.FullName -Directory -ErrorAction SilentlyContinue)) {
				$places.Add($sub.FullName)
			}
		}
	}
	foreach($place in $places) {
		if((Test-GameFolder $place) -and (Test-Path (Join-Path $place "images"))) { return $place }
	}
	return $null
}

function Find-Game {
	$candidates = New-Object System.Collections.Generic.List[string]
	$steamRoots = New-Object System.Collections.Generic.List[string]
	try {
		$p = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -ErrorAction Stop).SteamPath
		if($p) { $steamRoots.Add(($p -replace '/', '\')) }
	} catch {}
	foreach($key in @("HKLM:\SOFTWARE\WOW6432Node\Valve\Steam", "HKLM:\SOFTWARE\Valve\Steam")) {
		try {
			$p = (Get-ItemProperty -Path $key -ErrorAction Stop).InstallPath
			if($p) { $steamRoots.Add($p) }
		} catch {}
	}
	$steamRoots.Add("${env:ProgramFiles(x86)}\Steam")
	$steamRoots.Add("$env:ProgramFiles\Steam")
	foreach($root in $steamRoots) {
		$candidates.Add((Join-Path $root "steamapps\common\Endless Sky"))
		$vdf = Join-Path $root "steamapps\libraryfolders.vdf"
		if(Test-Path $vdf) {
			foreach($match in (Select-String -Path $vdf -Pattern '"path"\s+"([^"]+)"')) {
				$library = $match.Matches[0].Groups[1].Value -replace '\\\\', '\'
				$candidates.Add((Join-Path $library "steamapps\common\Endless Sky"))
			}
		}
	}
	$candidates.Add("$env:ProgramFiles\Endless Sky")
	$candidates.Add("${env:ProgramFiles(x86)}\Endless Sky")
	$candidates.Add("$env:LOCALAPPDATA\Programs\Endless Sky")
	foreach($candidate in $candidates) {
		if(Test-GameFolder $candidate) { return $candidate }
	}
	return $null
}

Say "Endless Sky Accessible installer."
# An official download extracted near this installer is the clearest sign of
# which game the player wants; then Steam; then other usual download places.
if(-not $GamePath) {
	$parent = Split-Path -Parent $here
	foreach($place in @($parent, (Split-Path -Parent $parent))) {
		if($place -and (Test-GameFolder $place) -and (Test-Path (Join-Path $place "images"))) { $GamePath = $place; break }
	}
}
if(-not $GamePath) { $GamePath = Find-Game }
if(-not $GamePath) { $GamePath = Find-DownloadedGame }
if(-not (Test-GameFolder $GamePath)) {
	Say "The Endless Sky game folder was not found automatically."
	Say "If you use the official download from GitHub, this is the folder you extracted EndlessSky-win64-v0.11.4.zip to."
	$GamePath = Ask "Type or paste the full path of your Endless Sky folder (the one containing credits.txt), then press Enter"
	$GamePath = $GamePath.Trim('"', ' ')
}
if(-not (Test-GameFolder $GamePath)) {
	Say "Error: $GamePath does not look like an Endless Sky folder. Installation cancelled."
	exit 1
}
Say "Using the game data in: $GamePath"

# The accessible build is Endless Sky 0.11.4; warn if the data is a different version.
$changelog = Join-Path $GamePath "changelog"
if(Test-Path $changelog) {
	$first = (Get-Content $changelog -TotalCount 1)
	if($first -notmatch "0\.11\.4") {
		Say "Warning: your Endless Sky data is '$first', but this accessible build is version 0.11.4."
		Say "Different versions may show data errors. Continue only if you understand this."
		$answer = Ask "Type Y and press Enter to continue, or just press Enter to cancel"
		if($answer -notmatch '^[Yy]') { Say "Installation cancelled."; exit 1 }
	}
}

if(-not $InstallDir) { $InstallDir = Join-Path $env:LOCALAPPDATA "Endless Sky Accessible" }
Say "Installing to: $InstallDir"
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item -Path (Join-Path $source "*") -Destination $InstallDir -Recurse -Force

# Tell the game where its data is (UTF-8 without a byte order mark).
$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText((Join-Path $InstallDir "resources-path.txt"), $GamePath, $utf8)

# Shortcuts on the desktop and in the Start menu.
$shell = New-Object -ComObject WScript.Shell
$targets = @(
	(Join-Path ([Environment]::GetFolderPath("Desktop")) "Endless Sky Accessible.lnk"),
	(Join-Path ([Environment]::GetFolderPath("Programs")) "Endless Sky Accessible.lnk")
)
foreach($link in $targets) {
	$shortcut = $shell.CreateShortcut($link)
	$shortcut.TargetPath = Join-Path $InstallDir "Endless Sky.exe"
	$shortcut.WorkingDirectory = $InstallDir
	$shortcut.Description = "Endless Sky with screen reader accessibility"
	$shortcut.Save()
}

# Copy the documentation next to the game.
$docs = Join-Path $here "docs"
if(Test-Path $docs) {
	Copy-Item -Path $docs -Destination $InstallDir -Recurse -Force
}

Say "Installation complete."
Say "Start the game with the shortcut named Endless Sky Accessible on your desktop or in the Start menu."
Say "Your original Endless Sky installation was not changed, and your saved games are shared with it."
