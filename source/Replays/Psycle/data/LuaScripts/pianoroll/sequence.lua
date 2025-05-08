--[[ psycle pianoroll (c) 2017 by psycledelics
File: sequence.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local player = require("psycle.player"):new()
local pattern = require("pattern")
local listener = require("psycle.listener")
local hostlistener = require("psycle.ui.hostactionlistener")
local eventposition = require("eventposition")

local sequence = {}

sequence.VIEWALLTRACKS = 5
sequence.VIEWSELTRACK = 6

function sequence:new()
  local c = {}
  setmetatable(c, self)
  self.__index = self
  c:init()
  return c
end

function sequence:init()
 self.trackviewmode = sequence.VIEWALLTRACKS
 self.listeners_ = listener:new()
 self.blockstart_ = nil
 self.blockend_ = nil
 self.orderlist = {}
end

function sequence:toggletrackviewmode()
  self.trackviewmode = self.trackviewmode == self.VIEWSELTRACK 
                       and self.VIEWALLTRACKS or self.VIEWSELTRACK
end

function sequence:bindtohost()
  self:update() 
  self:inithostlistener()
  return self
end

function sequence:update()
  self.playorder = player:playorder()
  local used = {}
  self.orderlist = {}
  self.startbeats = {}
  local startbeat = 0
  for i=1, #self.playorder do
    if not used[self.playorder[i]] then
      self.orderlist[i] = pattern:new(self.playorder[i])
      self.orderlist[i].changed:connect(sequence.onpatternchanged, self)
      used[self.playorder[i]] = self.orderlist[i]
    else
      self.orderlist[i] = used[self.playorder[i]]
    end     
    self.startbeats[i] = startbeat
    startbeat = startbeat + self.orderlist[i]:numbeats()  
  end 
  self.listeners_:notify(self, "onsequenceupdated")
  return self
end

function sequence:updateseqpos(seqpos)
  self:update()
end

function sequence:addlistener(listener)
  self.listeners_:addlistener(listener)
  return self
end

function sequence:inithostlistener()
  self.hostlistener = hostlistener:new()
  psycle.proxy:addhostlistener(self.hostlistener)
  local that = self
  function self.hostlistener:onticksperbeatchanged()   
    that:update()
  end      
end

function sequence:onpatternchanged(pattern)
  self.listeners_:notify(pattern, "onpatternchanged")
end

function sequence:notifypatternchange()
  self.listeners_:notify(pattern, "onpatternchanged")
end

function sequence:eraseselection()  
  for j=1, #self.orderlist do  
    self.orderlist[j]:eraseselection()
  end  
end

function sequence:selectall()  
  for j=1, #self.orderlist do
    self.orderlist[j]:selectall()
  end  
end

function sequence:deselectall()  
  for j=1, #self.orderlist do   
    self.orderlist[j]:deselectall()
  end   
end

function sequence:startbeat(pos)
  return self.startbeats[pos] and self.startbeats[pos] or 0
end

function sequence:at(pos)
  return self.orderlist[pos]
end

function sequence:patternindex(pos)  
  return self.playorder[pos]
end

function sequence:seqpos(ps)
  local result = nil
  for j=1, #self.orderlist do
    if self.orderlist[j] == ps then
      result = j
      break
    end
  end
  return result
end

function sequence:len()  
  return #self.orderlist
end

function sequence:selection()
  local result = {}
  for j=1, #self.orderlist do  
    local selected = self.orderlist[j]:selection()
    for k=1, #selected do
      result[#result + 1] = selected[k]
      selected[k].seqpos = j
    end
  end
  return result
end

function sequence:numbeats()
  local result = 0
  for i=1, #self.orderlist do   
    result = result + self.orderlist[i]:numbeats()  
  end 
  return result
end

function sequence:setblockstart(position, seqpos)
  self.blockstart_ = eventposition:new(position, 0, seqpos)
  self:selectblock()  
end

function sequence:setblockend(position, seqpos)
  self.blockend_ = eventposition:new(position, 0, seqpos)
  self:selectblock()
end

function sequence:selectblock()
  if self.blockstart_ and self.blockend_ then
    self:deselectall()
    local from, to = nil, nil
    if self.blockstart_:seqpos() == self.blockend_:seqpos() then
      if self.blockstart_:position() < self.blockend_:position() then
        from, to = self.blockstart_, self.blockend_
      else
        from, to = self.blockend_, self.blockstart_
      end
    elseif self.blockstart_:seqpos() < self.blockend_:seqpos() then
      from, to = self.blockstart_, self.blockend_
    else
      from, to = self.blockend_, self.blockstart_
    end    
    local events = {}
    for i=from:seqpos(), to:seqpos() do 
      local isstartseqpos, isendseqpos = from:seqpos() == i, to:seqpos() == i
      local pattern = self:at(i)
      local event = pattern:firstnote()
      while event do                   
        if (event:position() >= from:position() or not isstartseqpos) and
           (event:position() <= to:position() or not isendseqpos) then 
          if not events[i] then
            events[i] = {}
          end
          event:select(pattern)
        end
        event = event.next        
      end
    end   
  end
end

return sequence
