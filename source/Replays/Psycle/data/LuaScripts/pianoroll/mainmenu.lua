--[[ psycle pianoroll (c) 2017 by psycledelics
File: mainmenu.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local node = require("psycle.node")

local mainmenu = {}

function mainmenu:new(...)
  local m = {}  
  setmetatable(m, self)
  self.__index = self  
  m:init(...)
  return m
end

function mainmenu:init(selectioncommands)
  self.root = node:new():settext("Pianoroll")   
  node:new(self.root):settext("Note transposition")
      :add(node:new():settext("Transpose +1"):setcommand(selectioncommands.transpose1))
      :add(node:new():settext("Transpose -1"):setcommand(selectioncommands.transposeM1))
      :add(node:new():settext("Transpose +12"):setcommand(selectioncommands.transpose12))
      :add(node:new():settext("Transpose -12"):setcommand(selectioncommands.transposeM12)) 
  node:new(self.root):settext("Interpolate")
      :add(node:new():settext("Linear"):setcommand(selectioncommands.interpolatelinear))
      :add(node:new():settext("Curve"):setcommand(selectioncommands.interpolatecurve))
  node:new(self.root):settext("Block")
      :add(node:new():settext("Move"):setcommand(selectioncommands.move))
      :add(node:new():settext("Paste"):setcommand(selectioncommands.paste))
      :add(node:new():settext("Delete"):setcommand(selectioncommands.erase))
      :add(node:new():settext("Change Track"):setcommand(selectioncommands.changetrack))
      :add(node:new():settext("Change Generator"):setcommand(selectioncommands.changegenerator))
      :add(node:new():settext("Change Instrument"):setcommand(selectioncommands.changeinstrument))           
end

function mainmenu:rootnode()
  return self.root
end

return mainmenu
