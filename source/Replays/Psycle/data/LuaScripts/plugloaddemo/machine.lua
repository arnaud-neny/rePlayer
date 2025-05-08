--[[ 
psycle plugloaddemo (c) 2014 by psycledelics
File:  plugloaddemo/machine.lua.lua
Description: demo module that loads and uses a vst plugin inside
             freeverb3vst must be installed
copyright 2014 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

-- get machine base
machine = require("psycle.machine"):new()

-- plugin info
-- moved to register file /LuaScripts/plugloaddemo.lua

-- plugin constructor
function machine:init()   
   reverb = require("psycle.machine"):new("freeverb3vst-freeverb")
   p = require("orderedtable"):new()
   for i = 1, #reverb.params do
     p["fverb"..i] = reverb.params[i]
   end    
   self:addparameters(p);   
end

function machine:work(num)     
  reverb:setbuffer({self:channel(0),self:channel(1)});  
  reverb:work(num)  
end

return machine
