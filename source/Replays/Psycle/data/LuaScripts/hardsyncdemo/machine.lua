--[[ 
psycle hardsyncdemo (c) 2014 by psycledelics
File:  hardsyncdemo/machine.lua
Description: example for hardsync

copyright 2014 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

machine = require("psycle.machine"):new()

osc = require("psycle.osc");
param = require("psycle.parameter")
ahdsr = require("psycle.ahdsr")
dspmath = require("psycle.dsp.math")

function machine:info()
  return {vendor="psycle", name="hardsyncdemo", generator=1, version=0, api=0, noteon=1}
end

local osctypes = {"sin", "saw", "square", "tri"}
   
function machine:init(samplerate)  
   master = osc:new(osc.SQR, 263.1)
   slave = osc:new(osc.SAW, 263.1)
   slave:setsync(master)       
   noteon = false
   p = require("orderedtable").new()   
   p.md = param:newknob("Slave", "", 1, 2000, 1999, 263.1):addlistener(self)  
   p.st = param:newknob("Slave type", "", 1, 4, 3, 2):addlistener(self)  
   p.st.display = function(self) return osctypes[self:val()] end
   p.mt = param:newknob("Master type", "", 1, 4, 3, 3):addlistener(self)
   p.mt.display = function(self) return osctypes[self:val()] end
   p.fo = param:newknob("Fadeout Time", "Samples", 0, 128, 128, 32):addlistener(self)  
   p.mx = param:newknob("Mix", "Samples", 0, 100, 100, 0):addlistener(self)  
   self:addparameters(p)
   env = ahdsr:new(1,1,1,0.5,1)
   lastnote = 0
end

function machine:work(num)
   if env:isplaying() then 
     local amp = env:work(num)
     slave:work(self:channel(0))
	 local mix = p.mx:norm()
	 self:channel(0):mul(mix):add(slave.syncarray:mul(1-mix)):mul(amp)	 
	 self:channel(1):copy(self:channel(0))
   end
end

function machine:stop()
  env:fastrelease()
end

function machine:oncmd(command)

end

function machine:noteon(note)
   local f = dspmath.notetofreq(note)
   master:setfrequency(f)
   slave:start(0)	
   master:start(0)
   env:start()
   lastnote = note
end

function machine:noteoff(note)
   if lastnote == note then
     env:release()
   end
end

function machine:ontweaked(param)
  if param==p.md then
     slave:setfrequency(param:val())
  elseif param==p.mt then
     master:setshape(param:val())
  elseif param==p.st then
     slave:setshape(param:val())
  elseif param==p.fo then
     slave:setsyncfadeout(param:val())
  end
end

return machine
