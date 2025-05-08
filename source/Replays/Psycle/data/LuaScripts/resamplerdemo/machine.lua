--[[ 
psycle resamplerdemo (c) 2014 by psycledelics
File:  resamplerdemo/machine.lua
Description: creates a wavetable with 10 saws at 261 hz and
             a different set of harmonics
             additional a slide up from the noton pitch is added
copyright 2014 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

--require('mobdebug').start()

local machine = require("psycle.machine"):new()

local array = require("psycle.array");

-- plugin info
-- moved to register file /LuaScripts/resamplerdemo.lua

function machine.saw(num, maxharmonic)
  local data = array.new(num)  
  gain = 0.5
  local loop = array.arange(0, num, 1)
  for  h = 1, maxharmonic, 1 do
    amplitude = gain / h;
    to_angle = 2*math.pi / num * h;
    data:add(array.sin(math.pow(-1,h+1)*loop*to_angle)*amplitude)
  end
  return data
end

function machine.cwave(fh, sr)
   local f = 261.6255653005986346778499935233; -- C4
   local num = math.floor(sr/f + 0.5)         
   local hmax = math.floor(sr/2/fh)
   local data = machine.saw(num, hmax);
   wave = require("psycle.dsp.wavedata"):new()
   wave:copy(data)
   wave:setwavesamplerate(sr)
   wave:setloop(0, num)
   return wave
end

function machine:init(samplerate)  
   wavetable = {}   
   local flo = require("psycle.dsp.math").notetofreq(0)
   for i= 0, 10 do   
	 local fhi = flo*2
	 if i==0 then
	    flo = 0
	 end
	 local w = machine.cwave(fhi, samplerate)
     wavetable[#wavetable+1] = {w, flo, fhi}
	 flo = fhi
   end   
   resampler = require("psycle.dsp.resampler"):newwavetable(wavetable)
   resampler:setfrequency(263.1);   
   noteon = false
   env = require("psycle.envelope") 
end

function machine:work(num)
   if noteon then
     local a = envf:work(num);	 
     resampler:work(self:channel(0), self:channel(1), a)	 
	 --noteon = envf:isplaying()
   end
end

function machine:stop()
  resampler:noteoff()
  noteon = false
end

function machine:seqtick(channel, note, ins, cmd, val)    
   if note < 119 then      
      noteon = true
	  local f = require("psycle.dsp.math").notetofreq(note)
	  resampler:setfrequency(f);
	  resampler:start()
	  envf = env:new({{1, 900}},3);
   elseif note == 120 then
      resampler:noteoff()
	  noteon = false
   end	  
end

function machine:onsrchanged(rate)  
  self:init(rate)
end

return machine
