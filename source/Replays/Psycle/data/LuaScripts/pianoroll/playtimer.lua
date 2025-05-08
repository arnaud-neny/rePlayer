--[[ psycle pianoroll (c) 2017 by psycledelics
File: playtimer.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local player = require("psycle.player")
local listener = require("psycle.listener")

local playtimer = {}

function playtimer:new(...)
  local c = {}
  setmetatable(c, self)
  self.__index = self  
  c:init(...)
  return c
end

function playtimer:init(sequence)
  self.sequence = sequence 
  self.player = player:new()
  self.listeners_ = listener:new()  
  self.currplayline_, self.prevplayline_ = 0.0, 0.0
  self.currsequence_, self.prevsequence_ = 0, 0
  self.activeevents_, self.activemap_ = {}, {}
  self.automationevents_ = {}
  self.outdatedevents_, self.newevents_ = {}, {}
  self.newnote_ = false
end

function playtimer:addlistener(listener)
  self.listeners_:addlistener(listener)
  return self
end

function playtimer:start()
  local function f()
    self.currplayline_ = self.player:line()
    self.currsequence_ = self.player:playposition()
    if self.prevplayline_ ~= self.currplayline_ then
      self:updateactiveevents()   
      self.listeners_:notify(self, "onplaybarupdate")     
      self.prevplayline_ = self.currplayline_ 
      self.prevsequence_ = self.currsequence_      
    end
  end
  self.timerid = psycle.proxy:setinterval(f, 30) 
end

function playtimer:prevposition()
  return self.prevplayline_ / self.player:tpb()
end

function playtimer:currposition()
  return self.currplayline_ / self.player:tpb()
end

function playtimer:currsequence()
  return self.currsequence_
end

function playtimer:prevsequence()
  return self.prevsequence_
end

function playtimer:activemap()
  return self.activemap_
end

function playtimer:activeevents()
  return self.activeevents_
end

function playtimer:outdatedevents()
  return self.outdatedevents_
end

function playtimer:newevents()
  return self.newevents_
end

function playtimer:updateactiveevents()
  local pattern = self.sequence:at(self:currsequence() + 1)
  if pattern then
    local playposition = self:currposition() 
    self.activeevents_ = {}
    local event = pattern:firstnote()
    while event do       
      if playposition >= event:position() and playposition < event:position() + event:length()
         and event:note() >= 0 and event:note() < 120 then   
        self.activeevents_[#self.activeevents_ + 1] = event
        event.pattern = pattern  
      elseif event:hasstop() and playposition == event:position() + event:length() then    
        event.pattern = pattern      
      end      
      event = event.next
    end   
    local event = pattern:firstcmd()
    while event do  
      if playposition >= event:position() and playposition < event:position() + 1/self.player:tpb() then   
        self.activeevents_[#self.activeevents_ + 1] = event
        event.pattern = pattern      
      end
      event = event.next
     end             
  end
  self:calceventchange()
end

function playtimer:haschanged()  
  return #self.outdatedevents_ > 0 or #self.newevents_ > 0
end

function playtimer:calceventchange()
  local prev = self.activemap_  
  self.activemap_ = self:eventmap(self.activeevents_)
  self.outdatedevents_ = self:changed(self.activemap_, prev) 
  self.newevents_ = self:changed(prev, self.activemap_)
  self.newnote_ = self:hasnewnote(self.newevents_)
end

function playtimer:addautomation(ev)
  self.automationevents_[#self.automationevents_ + 1] = ev
end

function playtimer:clearautomation()
  self.automationevents_ = {}
end

function playtimer:automationevents()
  return self.automationevents_
end

function playtimer:eventmap(events)
  local result = {}
  for i=1, #events do
    local event = events[i]
    local note = event:note()
    if result[note] then
      result[note][#result[note] + 1] = event
    else
      result[note] = { event }
    end
  end
  return result
end

function playtimer:changed(map, source)
  local result = {}
  for note, events in pairs(source) do
    local entry = map[note]
    if not entry then
      for j=1, #events do
        result[#result + 1] = events[j]
      end
    else
      for j=1, #events do
        for i=1, #entry do
          if entry[i] ~= events[j] then     
            result[#result + 1] = events[j]     
          end
        end
      end
    end
  end
  return result
end

function playtimer:hasnewnote(events)
  local result = false
  for i=1, #events do
    if events[i]:isnote() then
      result = true
      break
    end
  end
  return result
end

function playtimer:playposition()
  local sequence = player:playposition()
  return sequence, self.trackview.patterns[sequence + 1].pattern:linetobeat(player:line())
end

function playtimer:isnoteplayed(event)
  return self:currposition() >= event:position() and self:currposition() < event:position() + event:length()
end

return playtimer
