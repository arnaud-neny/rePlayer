# Building rePlayer (another multi-formats music player)

Easy!:
- Install the latest Visual Studio (currently Visual Studio 2022).
- Open the solution rePlayer.sln from the source directory.
- Select your target and configuration.
- Build!

Issues:
- version 17.10.0 of Visual Studio introduced an issue with the std::mutex where the win32 runtime doesn't work on Windows 7 so rePlayer need the _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR define to make it work.