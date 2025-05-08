--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  patternevent.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local player = require("psycle.player"):new()
local machines = require("psycle.machine")
local rawpattern = require("psycle.pattern"):new()

local patternevent = {}

function patternevent:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function patternevent:init(note, pos, track, mach, inst)
  self.pos_ = 0.0
  self:set("mach_", mach, 0)
  self:set("inst_", inst, 255)
  self.cmd_ = 0
  self.parameter_ = 0
  self.stopoffset = 0  
  self:set("note_", note, 80)
  self:set("pos_", pos, 0.0)
  self:set("track_", track, 0)
  self.length_ = 0.5
  self.selected_ = nil
  self.prev, self.next = nil, nil
end

function patternevent:set(attrib, value, default)
  if value then
    self[attrib] = value
  else
    self[attrib] = default
  end
  return self
end

function patternevent:clone()
  local clone = patternevent:new(self.note_, self.pos_)
  clone.line = self.line
  clone.stopoffset = self.stopoffset
  clone.length_ = self.length_
  clone.track_ = self.track_
  clone.parameter_ = self.parameter_
  clone.cmd_ = self.cmd_
  clone.mach_ = self.mach_
  clone.inst_ = self.inst_
  clone.prev, clone.next = nil, nil  
  return clone
end

function patternevent:hasstop()
  return self.stopoffset ~= 0
end

function patternevent:raw()
  return {
    parameter = self.parameter_,
    cmd = self.cmd_,
    mach = self.mach_,
    inst = self.inst_,
    val = self.note_,
    track = self.track__
  }
end

function patternevent:setraw(ev)
  self.parameter_ = ev.parameter
  self.cmd_ = ev.cmd
  self.mach_ = ev.mach
  self.inst_ = ev.inst
  self.note_ = ev.val
  self.track_ = ev.track
  return self
end

function patternevent:setinst(inst)
  self.inst_ = inst
  return self
end

function patternevent:inst()
  return self.inst_
end

function patternevent:setmach(mach)
  self.mach_ = mach
  return self
end

function patternevent:mach()
  return self.mach_
end

function patternevent:settrack(track)
  self.track_ = track
  return self
end

function patternevent:track()
  return self.track_
end

function patternevent:setstopoffset(offset)
  self.stopoffset = offset
  self.length_ = offset
  return self
end

function patternevent:clearstop()
  self.stopoffset = 0
end

function patternevent:setlength(length)
  self.length_ = length
end  

function patternevent:setcmd(cmd)
  self.cmd_ = cmd
  return self
end

function patternevent:cmd()
  return self.cmd_
end

function patternevent:setnote(note)
  self.note_ = note
  return self
end

function patternevent:note()
  return self.note_
end

function patternevent:length()
  return self.length_
end

function patternevent:setposition(pos)
  self.pos_ = pos
  return self
end

function patternevent:position()
  return self.pos_
end

function patternevent:setparameter(parameter)
 self.parameter_ = parameter
 return self
end

function patternevent:parameter()
  return self.parameter_
end

function patternevent:tweakval()
  return self.parameter_ + self:cmd()*0xFF
end

function patternevent:setval16(val)
  self.parameter_ = math.min(255, val % 256)
  self.cmd_ = math.min(255, math.floor(val / 256))
  return self
end

function patternevent:setnorm(val)
  local steps, digits = self:steps() 
  if digits == 1 then
    self:setparameter(math.max(0, math.min(0xFF, math.floor(val * steps))))
  elseif digits == 2 then
    self:setval16(math.max(0, math.min(0xFFFF, math.floor(val * steps))))
  end
  return self
end

function patternevent:norm()
  local result = 0
  local steps, digits = self:steps() 
  if digits == 1 then
    result = self.parameter_ / steps
  elseif digits == 2 then
    result = (self.parameter_ + self:cmd()*0xFF) / steps
  end
  return result
end

function patternevent:equal(event)
  return event:parameter() == self.parameter_ and
         event:cmd() == self:cmd() and
         event.mach_ == self.mach_ and
         event.inst_ == self.inst_ and
         event.val == self.note
end

function patternevent:over(beat)
  return self.pos_ <= beat and beat <= self.pos_ + self.length_
end

function patternevent:overstop(beat)
  return self.stopoffset ~= 0 and self.pos_ + self.length_ <= beat and beat <= self.pos_ + self.length_ + 1/player:tpb()
end

function patternevent:hasstop()
  return self.stopoffset ~= 0
end

function patternevent:isnote()
  return self.note_ < 120
end

function patternevent:isstop()
  return self.note_ == 120
end

function patternevent:istwk()
  return self.note_ == 121
end

function patternevent:istws()
  return self.note_ == 124
end

function patternevent:ismcm()
  return self.note_ == 123
end

function patternevent:dump()
  psycle.output("Event trk, pos, key, stop, len "
      .. self.track_ .. "," 
      .. self.pos_ .. "," 
      .. self.note_ .. ","
      .. self.stopoffset .. ","
      .. self.length_ .. "\n")
end

function patternevent:selected()
  return self.selected_ ~= nil
end

function patternevent:selectedpattern()
  return self.selected_
end

function patternevent:select(pattern)
  self.selected_ = pattern
  return self
end

function patternevent:deselect()
  self.selected_ = nil
  return self
end

function patternevent:steps(event)
  local steps, digits = 65536, 2
  if self:isnote() then
    steps, digits = 256, 1
  else
    if self.mach_ ~= 255 then
      local minval, maxval, mactype = machines.paramrange(self.mach_, self.inst_)    
      if minval then    
        if mactype == machine.MACH_LUA or machine.MACH_VST then
          steps = 65536
        elseif maxval then
          steps = maxval - minval  
          if maxval < 255 and self:note() ~= 123 then
          digits = 1
          end        
        end      
      else
        if self:note() ~= 255 then
         steps = 0xFFFF       
        else
         steps = 256
         digits = 1
        end     
      end
    end
  end
  return steps, digits  
end

function patternevent:transpose(offset)
  self.note_ = self.note_ + offset
  return self
end

function patternevent:sync(pattern)
  local target = pattern and pattern or self.selected_
  if target and target.ps_ then
    rawpattern:setevent(target.ps_, self.track_, self.line, self:raw())
  end
  return self
end

return patternevent
