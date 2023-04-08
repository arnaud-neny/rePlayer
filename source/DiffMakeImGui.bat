cd /D %~dp0\..
git diff --no-index external\imgui-docking\imconfig.h source\Core\ImGui\imconfig.h > source\Core\ImGui\imgui.diff
git diff --no-index external\imgui-docking\imgui.h source\Core\ImGui\imgui.h >> source\Core\ImGui\imgui.diff
git diff --no-index external\imgui-docking\imgui_widgets.cpp source\Core\ImGui\imgui_widgets.cpp >> source\Core\ImGui\imgui.diff
git diff --no-index external\imgui-docking\backends\imgui_impl_win32.cpp source\Core\ImGui\imgui_impl_win32.cpp >> source\Core\ImGui\imgui.diff