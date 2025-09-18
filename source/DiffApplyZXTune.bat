@cd /D %~dp0\..
@git apply --unsafe-paths --ignore-whitespace source/Replays/ZXTune/ZXTune.diff
@cd source