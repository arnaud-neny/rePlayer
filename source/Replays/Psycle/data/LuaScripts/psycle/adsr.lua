-- ADSR Envelope Delegator (c) 2013/2014 by psycledelics
-- File: psycle/adsr.lua
-- copyright 2014 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local adsr = {}

local env = require("psycle.envelope")

function adsr:new(a, d, s, r, at, dt, rt)
  local e = {}      
  setmetatable(e, self)
  self.__index = self  
  if not at then at = env.LIN end
  if not dt then dt = env.LIN end
  if not rt then rt = env.LIN end
  e.env = env:new({{a, 1, at}, {d, s, dt}, {r, 0.001, rt}}, 3)
  e.r = r
  return e
end

function adsr:start() self:setrelease(self.r) self.env:start() end
function adsr:release() self.env:release() end
function adsr:fastrelease() self.env:settime(3, 0.03) self.env:release() end
function adsr:isplaying() return self.env:isplaying() end
function adsr:work(num) return self.env:work(num) end
function adsr:setattack(val) self.env:settime(1, val) end
function adsr:setdecay(val) self.env:settime(2, val) end
function adsr:setsustain(val) self.env:setpeak(2, val)end
function adsr:setrelease(val) self.r = val self.env:settime(3, val) end
function adsr:attacktime(val) self.env:time(1) end
function adsr:decaytime(val) self.env:time(2) end
function adsr:sustain(val) return self.env:peak(2)end
function adsr:releasetime(val) return self.env:time(3) end

return adsr