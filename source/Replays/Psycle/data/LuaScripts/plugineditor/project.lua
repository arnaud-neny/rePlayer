-- psycle plugineditor (c) 2016 by psycledelics
-- File: project.lua
-- copyright 2016 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version. 

local project = {}

function project:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self  
  m:init()
  return m
end

function project:init()   
  self.pluginindex_ = -1
end

function project:setplugininfo(plugininfo)
  self.plugininfo_ = plugininfo
  return self
end

function project:plugininfo()
  return self.plugininfo_
end

function project:setpluginindex(index)
  self.pluginindex_ = index
  return self
end

function project:pluginindex()
  return self.pluginindex_
end

return project