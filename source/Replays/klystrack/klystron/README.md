# klystron

This is a simple framework designed for remaking Thrust, the best game that was ever created.

So far the biggest project using the framework is [klystrack](https://github.com/kometbomb/klystrack), a chiptune tracker that uses the klystron sound engine for great success.

Some selling points:

* Sound synthesis for music and effects
* Low system requirements (the audio engine uses about 20 % CPU time on a 366 MHz Dingoo, at 22 Khz)
* Pixel-accurate sprite-sprite and sprite-background collision checking
* C99 (GNU99)
* SDL
* GUI stuff for simple mouse control 

In short: the feature set is something that could be found inside an generic video game console circa 1991. The API is relatively simple, yet uses some (stupid) tricks. The main goal is to provide pixel-accurate collisions which is a very important feature for a thrustlike game. The synth and the music routine are thrown in just for fun. 
