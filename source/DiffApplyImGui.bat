@cd /D %~dp0\..
@git apply --unsafe-paths --ignore-whitespace source/Core/ImGui/imgui.diff
@cd source