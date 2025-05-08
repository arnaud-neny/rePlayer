--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  statehandler.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local listeners = require("psycle.listener")

statehandler = {}

function statehandler:new(...)
  local m = {}                
  setmetatable(m, self)
  self.__index = self    
  m:init(...)  
  return m
end

function statehandler:run(...)
  if self.state_ and self.state_.enter then
     self.state_:enter(...)
  end
end

function statehandler:init(startstate)
  self.oldstatustext = ""
  self.state_ = startstate
  self.statuslisteners_ = listeners:new()
end

function statehandler:setstate(state, ...)
  self.state_ = state
  self:run(...)
  return self
end

function statehandler:transition(f, ...)  
  if self.state_ and self.state_[f] then
    self:notifystatus(...)
    local nextstate = self.state_[f](self.state_, ...) 
    if nextstate then
      if self.state_ and self.state_.exit then        
        self.state_:exit(...)
      end
      self.state_ = nextstate
      if self.state_.enter then
        self.state_:enter(...)
      end
      self:notifystatus(...)
    end
  end
  return self
end

function statehandler:addstatuslistener(listener)
  self.statuslisteners_:addlistener(listener)
  return sef
end

function statehandler:notifystatus(...)
  if self.oldstatustext ~= self:statustext(...) then 
    self.statuslisteners_:notify(self:statustext(...), "onstatus")
    self.oldstatustext = self:statustext(...)    
  end
  return self  
end

function statehandler:statustext(...)
  local result = nil
  if self.state_ and self.state_.statustext then
    result = self.state_:statustext(...)
  end  
  if not result then
    result = ""
  end
  return result
end

function statehandler:onmouseenter(...)
  self.oldstatustext = self:statustext(...)
  self.statuslisteners_:notify(self:statustext(...), "onstatus")     
end

return statehandler
