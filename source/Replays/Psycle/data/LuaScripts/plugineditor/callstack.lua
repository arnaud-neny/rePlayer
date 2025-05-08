-- psycle callstack (c) 2015, 2016 by psycledelics
-- File: callstack.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local listview = require("psycle.ui.listview")
local node = require("psycle.node")
local signal = require("psycle.signal")

local callstack = listview:new()

function callstack:new(parent)
  local c = listview:new(parent)  
  setmetatable(c, self)
  self.__index = self  
  c:init()  
  return c
end

function callstack:init()   
  self:addcolumn("", 50)
      :addcolumn("Name", 400)  
      :addcolumn("Source", 200)  
      :setautosize(false, false)
      :viewreport()
      :enablerowselect()    
  self.rootnode = node:new()  
  self:setrootnode(self.rootnode)  
  self.change = signal:new()
end

function callstack:addline(info)
  local item = node:new():settext("")  
  local infocopy = {}  
  infocopy.name = info.name
  infocopy.line = info.line
  infocopy.source = info.source  
  local subitem = node:new():settext(info.name.." Line "..info.line)  
  item:add(subitem)
  local subitem1 = node:new():settext(info.source:match("([^\\]+)$"))  
  subitem1.info = infocopy
  subitem:add(subitem1)  
  self.rootnode:add(item)  
end

function callstack:onchange(node)  
  self.change:emit(node.info)
end

return callstack