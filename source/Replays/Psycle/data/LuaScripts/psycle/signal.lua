-- psycle plugineditor (c) 2017 by psycledelics
-- File: signal.lua
-- copyright 2017 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.

local signal = {}

function signal:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m.listenerfunc_  = {}
  m.listenerinstance_ = {}
  setmetatable(m.listenerfunc_, {__mode ="kv"})  -- weak table  
  setmetatable(m.listenerinstance_, {__mode ="kv"})  -- weak table
  m.prevented_ = false
  return m
end

function signal:connect(func, instance) 
  local found = false
  for k=1, #self.listenerfunc_ do      
    if (self.listenerinstance_[k] == instance and
        self.listenerfunc_[k] == func) then	   
      found = true
	    break
	  end
  end
  if not found then
    self.listenerfunc_[#self.listenerfunc_ + 1] =  func
    self.listenerinstance_[#self.listenerinstance_ + 1] = instance
  end   
  return self  
end

function signal:emit(...) 
  if not self.prevented_ then 
    for k = 1, #self.listenerfunc_ do    
      self.listenerfunc_[k](self.listenerinstance_[k], ...)
    end
  end
  return self
end

function signal:prevent()
  self.prevented_ = true
  return self
end

function signal:enable()
  self.prevented_ = false
  return self
end

function signal:prevented()
  return self.prevented_
end

return signal
