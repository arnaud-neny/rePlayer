# Building rePlayer (another multi-formats music player)

Easy!:
- Install the latest Visual Studio (currently Visual Studio 2022).
- Open the solution rePlayer.sln from the source directory.
- Select your target and configuration.
- Build!

NB: there is an [issue](https://developercommunity.visualstudio.com/t/Using-mutex-throws-an-exception/10667647) from the version 17.10.0 of Visual Studio where the win32 runtime doesn't work on Windows 7 because of a regression in the stl (mutex is using a function not available on this system).  
So right now, the win32 release is build from a Visual Studio 2022 LTSC 17.6.15.