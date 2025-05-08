        ___________________________
       /|                         |\
      / | JM Drum  v. 2.5         | \
     /  | Developed by [JAZ]      |  \
    /___|                         |___\
   /\   | For comments/whatever   |   /\
  /\/\  | mail to:                |  /\/\
 /  \/\ |   jaz@pastnotecut.org   | /\/  \
/____\/\| Code Donated to the     |/\/____\
\     \/| "Psycle Community"      |\/     /
/______\|_________________________|/______\


------------------------------------------


Index
-------

 . 1) Introduction
 · 2) Parameters
 · 3) Nice Configurations
 · 4) Changes History
 · 5) What will come in a new version?
 · 6) End Notes


1) Introduction
-----------------

JM Drum is a Psycle Plugin that emulates a Drum. It is
highly configurable and, alone or with Arguru Distortion
or even Yezar Freeverb, reproduces a wide range of Drums.


2) Parameters
---------------

StartFreq: Frequency at which drum starts to play.
EndFreq : Frequency at which drum ends playing.
FrecDecay : Time elapsed between StartFreq and Endfreq.
StartAmp : Volume when beginning playback. (Mode 1.x)
EndAmp : Volume when ending playback. (Mode 1.x)
         Note: Volume goes from StartAmp to Endamp in a linear way.

Length : Time that drum plays.
Volume : Volume of the drum.
DecMode : Mode of decrementing. (Linear, Mode 1.1,...)
Compatible: Selection of Compatibility Mode (Older version or newer version).
NNA : Select NoteOff or NoteContinue when New Note on same Channel.

Attack : Percentage at which Drum volume will have gone up to MAX.
Decay : Percentage at which Drum volume will have gone down to Sustain.
Sustain : Percentage of volume at Sustain Pos.
Mix : Mix percentage of Drum and Thump.
Thump Length : Length of the Thump.
Thump Freq : Frequency at which the Thump is played.

 - Center Note is C-4 -
   ~~~~~~~~~~~~~~~~~~


3) Nice Configurations
------------------------

 JMDrum is "very high configurable and cool experimenting" (Gerwin says).
 Here you have some good configurations that me (and users) have ended with.
 ( Missing parameters/notes are supposed to be at default value )

DRUMS:

  · DecMode= Mode 2.0
    Compatibility= Version 2.x
    Drum&Thump Mix= 100%:63%

  · StartFreq= 210
    Endfreq= 55
    Freq Decay= 55
    Length= 247
    Volume= 21844
    DecMode = Mode 2.0
    Compatibility=Version 2.x
    Decay= 71
    Sustain = 38
    Mix = 100%:53%
    ThumpLen = 1.5s

TOMS:

  · StartFreq= 600
    EndFreq= 200 
    FreqDecay= 10
    Length= 295
    Volume 25000~30000
    DecMode = Mode 2.0
    Compatibility= Version 2.x
    Decay= 38%
    Sustain= 24%
    Drum&Thump Mix= 100%:65%
    ThumpLen= 3.0s
    Notes: C-3 E-3 G-3 C-4
    
  · StartFreq= 220 
    Length= 248
    DecMode = No Decrease
    Compatibility= Version 2.x
    Drum&Thump Mix= 68%:100%
    Volume= 10000 ~ 15000
    Notes: C-3 E-3 G-3 C-4

    
  · Where's yours?!!? :·D

  · Have you tried to use Arguru Distortion? Do it!!


4) Changes History 
--------------------

·New in 2.2:

 - Fixed Clip-when-unmuting-after-note-played bug (more o less)
 - Added Thump Frequency as parameter. (from 220Hz to 6000Hz)
 - Changed Thump Length Scale and Margins. Now from 0.1s to 6.0s
   in 0.1s steps.
 - Not fully compatible with 2.1.x due to this change.
   (Take a look at the notes at the end of the file)


·New in 2.1.3:

 - Changed Thump Frequency (again) from 4.4Khz to 440Hz
   It is usable now! (2.0s set as the default Length)
 - A few optimizations here and there (A little faster)
 - Cleaned source code. Removed all unused/not working code.
 - Changed a bit the default parameters.
 

·New in 2.1.2:

 - Notes! (yep! finally here) Center note is C-4
 - Made GetSample() funtion inline (this means plugin is faster)
 - Changed some compiler parameters. Dll size is now 
   like the other psycle machines!
 - Lowered Thump Frequency a bit. I think it sounds better now.


·New in 2.1.1:

- This version is only a bugfix. When 24 notes are played at the same time,
  2.1.0 does weird things and even crashes Psycle!  (easiest way to reproduce
  it is muting the machine)


·New in 2.1.0:

- Redesigned.
  · Drum Generator: 
    -Based on my "Envelope" class. (Renamed to Coscillator in 2.1.3) (*)
    -Added another Decreasing Formula. (*)
    -Added Thump!
    -Optimizations (*)
    -Sounds COOOOOOOOOOOLER! (1.1 low frequencies were quite bad in fact) (*)
  · Machine Interface:
    -Moved most of the calculations from NoteOn to ParameterTweak.
     (More calcs to do, but less often)
    -Added Channel-NoteOff! Before, new Notes on the same channel cut
     previous ones.
    -Added NNA Parameter to select between NoteOff or NoteContinue
    -Now only up to 24 notes at the same time. (Before they were 32, one
     for each channel) This means it takes less memory. (*)
    -command 0Cxx has changed its limits in Mode 2.x. (*)

- Changed Most of the parameters
  · Exponential Mode renamed to DecMode and added 2 new modes.
  · Decay Renamed to Length. Much more obvious. (*)
  · StartAmp and EndAmp only work in Mode 1.x
  · Added Attack, Decay and Sustain. They work in Mode 2.x. (*)
    This allow much more configurability.
  · Obvioulsy, the Compatibility selector
  · NNA's
  · Mix Control. (To mix Thump&Drum)
  · Thump Lenght

- New Default parameters

(*) - Changes already present in beta version.

·New in version 1.1

- Exponential Mode! Your drums will sound
  more realystic.
- Tweaked a little the default parameters.


·1.0 First release

- "This is really a 'test' more than a plugin,
   but it runs pretty well." (/me said ;·D)



5) What may come in a new version?
------------------------------------

  I hope I will leave the JMDrum as it is now. I think there's
  not much that could be changed.


6) End Notes
--------------

- Version 2.2 is not fully compatible with the 2.1.x series
  (that in fact weren't fully compatible with themselves)
  When loading a song that uses one of them:
   · multiply the ThumpLenght by 5 and
   · change the ThumpFrequecy to:
     5000Hz if it is 2.1.0 or 2.1.1
     4400Hz if it is 2.1.2
     440Hz  if it is 2.1.3

- All 2.x versions have a bug with the Decrease Mode 1.1. They don't stop at
  endFreq. It appears especially with small FrecDecays, because it decreases
  too fast and even goes back to startfreq.
  I'm not gonna fix this because it would pump up the cpu usage and because
  it's a little used mode.

- This version is intended to replace the 1.x and 2.1.x series.
  Of course, you can rename 1.x to JMDrums1.dll, 2.1 to JMDrums2.dll
  and this one to JMDrums22.dll, but remember to save songs
  with JMDrum.dll.

- Default parameters on Versions 2.x are set for compatibiliby with 1.x
  versions. If you want to hear how good the 2.x versions sound, change
  DecMode to "Mode 2.0", compatibility to "mode 2.x" and
  Thump&Drum Mix to something of your likely :·)

- Version 2.1.3 is the fastest of all JMDrum's. 2.2 is almost as fast.
