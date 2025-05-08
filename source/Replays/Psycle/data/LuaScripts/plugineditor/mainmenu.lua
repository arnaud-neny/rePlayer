--[[ psycle plugineditor (c) 2016, 2017 by psycledelics
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

function mainmenu:init(filecommands, editcommands, runcommands)
  self.root = node:new():settext("Plugineditor")   
  node:new(self.root):settext("File")
      :add(node:new():settext("New"):setcommand(filecommands.newpage))
      :add(node:new():settext("Open"):setcommand(filecommands.loadpage))
      :add(node:new():settext("Save"):setcommand(filecommands.saveresume))
      :add(node:new():settext("Close"):setcommand(filecommands.savedone))       
  node:new(self.root):settext("Edit")
      :add(node:new():settext("Undo"):setcommand(editcommands.undopage))
      :add(node:new():settext("Redo"):setcommand(editcommands.redopage))
      :add(node:new():settext("Cut"):setcommand(editcommands.cut))
      :add(node:new():settext("Copy"):setcommand(editcommands.copy))
      :add(node:new():settext("Paste"):setcommand(editcommands.paste))
      :add(node:new():settext("Delete"):setcommand(editcommands.delete))
      :add(node:new():settext("Sel All"):setcommand(editcommands.selall))
      :add(node:new():settext("Block")
               :add(node:new():settext("Delete")
                        :setcommand(editcommands.deleteblock)))
  node:new(self.root):settext("Run")
      :add(node:new():settext("Run"):setcommand(runcommands.run))                         
end

function mainmenu:rootnode()
  return self.root
end

return mainmenu
