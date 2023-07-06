@echo off
setlocal enabledelayedexpansion
cd /D %~dp0\..
copy NUL source\Replays\ZXTune\ZXTune.diff
set curPath=%cd%
set endPath=\source\Replays\ZXTune
set relPath=%curPath%%endPath%
set extPath=external
echo rpath !relPath!
for /r "source\Replays\ZXTune\include\" %%x in (*.*) do (
	set currentFile=%%x
	set currentFile=!currentFile:%relPath%=!
	echo Relative !currentFile!
	git diff --no-index external\zxtune!currentFile! source\Replays\ZXTune!currentFile! >> source\Replays\ZXTune\ZXTune.diff
)
for /r "source\Replays\ZXTune\src\" %%x in (*.*) do (
	set currentFile=%%x
	set currentFile=!currentFile:%relPath%=!
	echo Relative !currentFile!
	git diff --no-index external\zxtune!currentFile! source\Replays\ZXTune!currentFile! >> source\Replays\ZXTune\ZXTune.diff
)