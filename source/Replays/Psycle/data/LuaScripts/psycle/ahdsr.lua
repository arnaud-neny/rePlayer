-- ADSR Envelope Delegator (c) 2013/2014 by psycledelics
-- File: psycle/ahdsr.lua
-- copyright 2014 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.

local ahdsr = {}

local env = require("psycle.envelope")

function ahdsr:new(a, h, d, s, r, f)
  local e = {}      
  setmetatable(e, self)
  self.__index = self  
  e.env = env:new({{a, 1, f}, {h, 1}, {d, s, f}, {r, 0.001, f}}, 4)
  e.r = r
  return e
end

function ahdsr:start() self:setrelease(self.r) self.env:start() end
function ahdsr:release() self.env:release() end
function ahdsr:fastrelease() self.env:settime(4, 0.03) self.env:release() end
function ahdsr:isplaying() return self.env:isplaying() end
function ahdsr:work(num) return self.env:work(num) end
function ahdsr:setattack(val) self.env:settime(1, val) end
function ahdsr:sethold(val) self.env:settime(2, val) end
function ahdsr:setdecay(val) self.env:settime(3, val) end
function ahdsr:setsustain(val) self.env:setpeak(3, val)end
function ahdsr:setrelease(val) self.r = val self.env:settime(4, val) end
function ahdsr:attacktime(val) self.env:time(1) end
function ahdsr:holdtime(val) self.env:time(2) end
function ahdsr:decaytime(val) self.env:time(3) end
function ahdsr:sustain(val) return self.env:peak(3)end
function ahdsr:releasetime(val) return self.env:time(4) end

return ahdsr