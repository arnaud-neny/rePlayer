-- psycle plugineditor (c) 2015 by psycledelics
-- File: listener.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  


local listener = {}

function listener:new(methodname)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m.listener_ = {}    
  setmetatable(m.listener_, {__mode ="kv"})  -- weak table  
  m.methodname_ = methodname
  m.prevented_ = false
  return m
end

function listener:addlistener(listener)
  local found = false
  for k=1, #self.listener_ do      
    if (self.listener_[k]==listener) then	   
      found = true
	    break
	  end
  end
  if not found then
    self.listener_[#self.listener_+1]=listener
  end   
  return self
end

function listener:notify(val, method)
  if not self.prevented_ then
    if method==nil then
      method = self.methodname_
    end    
    for k, v in pairs(self.listener_) do    
      local m = v[method]
      if m ~= nil then      
        m(v, val)
      end	  
    end
  end
  return self
end

function listener:enable()
  self.prevented_ = false
  return self
end

function listener:prevent()
  self.prevented_ = true
  return self
end

function listener:isprevented()
  return self.prevented_
end

return listener
