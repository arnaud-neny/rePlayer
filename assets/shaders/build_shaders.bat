@echo off
cls

set dxc=..\..\tools\dxc.exe

%dxc% -T vs_5_0 imgui.hlsl /E MainVertexShader /Vn gImGuiVS -Fh ../../\source/Graphics/GraphicsImGuiVS.h
%dxc% -T ps_5_0 imgui.hlsl /E MainPixelShader /Vn gImGuiPS -Fh ../../\source/Graphics/GraphicsImGuiPS.h

%dxc% -T vs_5_0 premul.hlsl /E MainVertexShader /Vn gPremulVS -Fh ../../\source/Graphics/GraphicsPremulVS.h
%dxc% -T ps_5_0 premul.hlsl /E MainPixelShader /Vn gPremulPS -Fh ../../\source/Graphics/GraphicsPremulPS.h
