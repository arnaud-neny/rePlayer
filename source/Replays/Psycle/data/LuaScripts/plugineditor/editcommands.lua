--[[ psycle plugineditor (c) 2017 by psycledelics
File: textpagecommands.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local boxspace = require("psycle.ui.boxspace")
local command = require("psycle.command")
local textpage = require("textpage")
local filesave = require("psycle.ui.filesave")
local fileopen = require("psycle.ui.fileopen")

local blockbegincommand = command:new()

function blockbegincommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function blockbegincommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():setblockbegin():setfocus()
  end
end

local blockendcommand = command:new()

function blockendcommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer  
  return c
end

function blockendcommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():setblockend():setfocus()
  end
end

local blockdeletecommand = command:new()

function blockdeletecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function blockdeletecommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():deleteblock():setfocus()
  end
end

local blockcopycommand = command:new()

function blockcopycommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function blockcopycommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():copyblock():setfocus()
  end
end

local blockmovecommand = command:new()

function blockmovecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function blockmovecommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():moveblock():setfocus()
  end
end

local charleftcommand = command:new()

function charleftcommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function charleftcommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():charleft():setfocus()
  end
end

local charrightcommand = command:new()

function charrightcommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function charrightcommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():charright():setfocus()
  end
end

local charleftcommand = command:new()

function charleftcommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function charleftcommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():charleft():setfocus()
  end
end

local lineupcommand = command:new()

function lineupcommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function lineupcommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():lineup():setfocus()
  end
end

local linedowncommand = command:new()

function linedowncommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function linedowncommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():linedown():setfocus()
  end
end

local wordleftcommand = command:new()

function wordleftcommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function wordleftcommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():wordleft():setfocus()
  end
end

local wordrightcommand = command:new()

function wordrightcommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function wordrightcommand:execute()
  if self.pagecontainer_:activepage() then   
    self.pagecontainer_:activepage():wordright():setfocus()
  end
end

local undopagecommand = command:new()

function undopagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function undopagecommand:execute()
  local page = self.pagecontainer_:activepage()
  if page and page.undo then
    page:undo()
  end
end

local redopagecommand = command:new()

function redopagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function redopagecommand:execute()
  local page = self.pagecontainer_:activepage()
  if page and page.redo then
    page:redo():setfocus()
  end
end

local cutpagecommand = command:new()

function cutpagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function cutpagecommand:execute()
  if self.pagecontainer_:activepage() then
    self.pagecontainer_:activepage():cut()
  end
end

local copypagecommand = command:new()

function copypagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function copypagecommand:execute()
  if self.pagecontainer_:activepage() then
    self.pagecontainer_:activepage():copy():setfocus()
  end
end

local pastepagecommand = command:new()

function pastepagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function pastepagecommand:execute()
  if self.pagecontainer_:activepage() then
    self.pagecontainer_:activepage():paste():setfocus()
  end
end

local deletepagecommand = command:new()

function deletepagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function deletepagecommand:execute()
  if self.pagecontainer_:activepage() then
    self.pagecontainer_:activepage():deletesel():setfocus()
  end
end

local selallpagecommand = command:new()

function selallpagecommand:new(pagecontainer)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.pagecontainer_ = pagecontainer
  return c
end

function selallpagecommand:execute()
  if self.pagecontainer_:activepage() then
    self.pagecontainer_:activepage():selall():setfocus()
  end
end

local commandfactory = {}

commandfactory.SETBLOCKBEGIN = 1
commandfactory.SETBLOCKEND = 2
commandfactory.DELETEBLOCK = 3
commandfactory.COPYBLOCK = 4
commandfactory.MOVEBLOCK = 5
commandfactory.CHARLEFT = 6
commandfactory.CHARRIGHT = 7
commandfactory.LINEUP = 8
commandfactory.LINEDOWN = 9
commandfactory.WORDLEFT = 10
commandfactory.WORDRIGHT = 11
commandfactory.REDOPAGE = 12
commandfactory.UNDOPAGE = 13
commandfactory.CUT = 14
commandfactory.COPY = 15
commandfactory.PASTE = 16
commandfactory.DELETE = 17
commandfactory.SELALL = 18

function commandfactory.build(cmd, ...)
  local result = nil
  if cmd == commandfactory.SETBLOCKBEGIN then
    result = blockbegincommand:new(...)
  elseif cmd == commandfactory.SETBLOCKEND then
    result = blockendcommand:new(...)
  elseif cmd == commandfactory.COPYBLOCK then
    result = blockcopycommand:new(...)
  elseif cmd == commandfactory.MOVEBLOCK then
    result = blockmovecommand:new(...)
  elseif cmd == commandfactory.DELETEBLOCK then
    result = blockdeletecommand:new(...)  
  elseif cmd == commandfactory.CHARLEFT then
    result = charleftcommand:new(...)  
  elseif cmd == commandfactory.CHARRIGHT then
    result = charrightcommand:new(...)
  elseif cmd == commandfactory.LINEUP then
    result = lineupcommand:new(...)
  elseif cmd == commandfactory.LINEDOWN then
    result = linedowncommand:new(...)   
  elseif cmd == commandfactory.WORDLEFT then
    result = wordleftcommand:new(...)
  elseif cmd == commandfactory.WORDRIGHT then
    result = wordrightcommand:new(...)   
  elseif cmd == commandfactory.REDOPAGE then
    result = redopagecommand:new(...)
  elseif cmd == commandfactory.UNDOPAGE then
    result = undopagecommand:new(...)
  elseif cmd == commandfactory.CUT then
    result = cutpagecommand:new(...)
  elseif cmd == commandfactory.COPY then
    result = copypagecommand:new(...)  
  elseif cmd == commandfactory.PASTE then
   result = pastepagecommand:new(...)
  elseif cmd == commandfactory.DELETE then
   result = deletepagecommand:new(...)
  elseif cmd == commandfactory.SELALL then
   result = selallpagecommand:new(...)
  end
  return result
end

return commandfactory