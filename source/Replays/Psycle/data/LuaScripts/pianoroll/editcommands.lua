--[[ psycle pianoroll (c) 2017 by psycledelics
File: gridcommands.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.  
]]

local command = require("psycle.command")
local sequencebar = require("psycle.sequencebar"):new()
local rawpattern = require("psycle.pattern")
local player = require("psycle.player"):new()

local selectnotecommand = command:new()

function selectnotecommand:new(trackview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self
  c.trackview = trackview
  c.patternevent = patternevent
  return c
end

function selectnotecommand:execute(patternevent)
  self.trackview.track_ = patternevent:track()
  self.trackview:fls()
end

local editmodecommand = command:new()

function editmodecommand:new(patternview, mainview, icons)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  c.mainview = mainview
  c.icons = icons
  return c
end

function editmodecommand:execute() 
  self.patternview.selectionrect = nil
  self.patternview:setinputmode(self.patternview.EDITMODE)
  self.patternview.controlview:setinputmode(self.patternview.EDITMODE)
  self.icons.edit:seton(true)
  self.mainview.dragtoolbar:show()
  self.mainview.toolbar:updatealign()
end

local selectmodecommand = command:new()

function selectmodecommand:new(patternview, mainview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  c.mainview = mainview
  return c
end

function selectmodecommand:execute() 
  self.patternview:setinputmode(self.patternview.SELECTMODE)
  self.patternview.controlview:setinputmode(self.patternview.SELECTMODE)
  self.mainview.dragtoolbar:hide()
  self.mainview.toolbar:updatealign()
end

local slicemodecommand = command:new()

function slicemodecommand:new(patternview, mainview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  c.mainview = mainview
  return c
end

function slicemodecommand:execute() 
  self.patternview:setinputmode(self.patternview.SLICEMODE)
  self.patternview.controlview:setinputmode(self.patternview.SLICEMODE)
  self.mainview.dragtoolbar:hide()
  self.mainview.toolbar:updatealign()
end

local copymodecommand = command:new()

function copymodecommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function copymodecommand:execute() 
  self.patternview:setdragmode(self.patternview.COPYMODE)
end

local movemodecommand = command:new()

function movemodecommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function movemodecommand:execute() 
  self.patternview:setdragmode(self.patternview.MOVEMODE)
end

local trackviewmodecommand = command:new()

function trackviewmodecommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function trackviewmodecommand:execute() 
  self.patternview:toggletrackviewmode()
end

local trackeditmodecommand = command:new()

function trackeditmodecommand:new(trackview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.trackview = trackview
  return c
end

function trackeditmodecommand:execute() 
  self.trackview:toggletrackeditmode()
end

local keyboardmodecommand = command:new()

function keyboardmodecommand:new(keyboard, icon)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.keyboard = keyboard
  c.icon = icon
  return c
end

function keyboardmodecommand:execute() 
   self.keyboard:toggledisplay()
   self.icon:settext(self.keyboard:displayname()) 
end

local hzoomincommand = command:new()

function hzoomincommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function hzoomincommand:execute() 
  self.patternview:zoom():dechzoom()
end

local hzoomoutcommand = command:new()

function hzoomoutcommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function hzoomoutcommand:execute() 
  self.patternview:zoom():inchzoom()
end

local hzoomfitcommand = command:new()

function hzoomfitcommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function hzoomfitcommand:execute() 
  local numlines = self.patternview.sequence:at(sequencebar:editposition() + 1):numlines()
  self.patternview:zoom():fithzoom(numlines, self.patternview:position():width())
end

local vzoomincommand = command:new()

function vzoomincommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function vzoomincommand:execute() 
  self.patternview:preventdraw()
  self.patternview:zoom():decvzoom()
  self.patternview:enabledraw()
  self.patternview.scroller:restoremidkey()
end

local vzoomoutcommand = command:new()

function vzoomoutcommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function vzoomoutcommand:execute()
  self.patternview:preventdraw()
  self.patternview:zoom():incvzoom()
  self.patternview:enabledraw()
  self.patternview.scroller:restoremidkey()
end

local vzoomfitcommand = command:new()

function vzoomfitcommand:new(patternview)
  local c = command:new()  
  setmetatable(c, self)
  self.__index = self  
  c.patternview = patternview
  return c
end

function vzoomfitcommand:execute() 
  self.patternview:zoom():fitvzoom(self.patternview.keyboard.keymap:numkeys(), self.patternview:position():height())
end

local zoomfitcommand = command:new()

function zoomfitcommand:new(patternview)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self  
  m.patternview = patternview
  return m
end

function zoomfitcommand:execute() 
  local numlines = self.patternview.sequence:at(sequencebar:editposition() + 1):numlines()
  self.patternview:zoom():fithzoom(numlines, self.patternview:position():width())
  self.patternview:zoom():fitvzoom(self.patternview.keyboard.keymap:numkeys(), self.patternview:position():height())
end

local undocommand = command:new()

function undocommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self
  m:init(...)
  return m
end

function undocommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function undocommand:execute()
  local column = 0 
  rawpattern.undo(
    self.cursor_:track(),
    math.floor(self.cursor_:position() * player:tpb()),
    column,
    self.cursor_:seqpos()
  )
  self.sequence_:update()
end

local redocommand = command:new()

function redocommand:new(...)
  local m = command:new()  
  setmetatable(m, self)
  self.__index = self
  m:init(...)
  return m
end

function redocommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function redocommand:execute()
  local column = 0
  rawpattern.redo(
    self.cursor_:track(),
    math.floor(self.cursor_:position() * player:tpb()),
    column)
  self.sequence_:update()
end

local commandfactory = {}

commandfactory.SELECTNOTE = 1
commandfactory.EDITMODE = 2
commandfactory.SELECTMODE = 3
commandfactory.SLICEMODE = 4
commandfactory.MOVEMODE = 5
commandfactory.COPYMODE = 6
commandfactory.TRACKVIEWMODE = 7
commandfactory.TRACKEDITMODE = 8
commandfactory.KEYBOARDMODE = 9
commandfactory.HZOOMIN = 10
commandfactory.HZOOMOUT = 11
commandfactory.HZOOMFIT = 12
commandfactory.VZOOMIN = 13
commandfactory.VZOOMOUT = 14
commandfactory.VZOOMFIT = 15
commandfactory.ZOOMFIT = 16
commandfactory.UNDO = 17
commandfactory.REDO = 18



function commandfactory.build(cmd, ...)
  local result = nil
  if cmd == commandfactory.SELECTNOTE then
    result = selectnotecommand:new(...)
  elseif cmd == commandfactory.EDITMODE then
    result = editmodecommand:new(...)
  elseif cmd == commandfactory.SELECTMODE then
    result = selectmodecommand:new(...)
  elseif cmd == commandfactory.SLICEMODE then
    result = slicemodecommand:new(...)    
  elseif cmd == commandfactory.MOVEMODE then
    result = movemodecommand:new(...)
  elseif cmd == commandfactory.COPYMODE then
    result = copymodecommand:new(...)
  elseif cmd == commandfactory.TRACKVIEWMODE then
    result = trackviewmodecommand:new(...)
  elseif cmd == commandfactory.TRACKEDITMODE then
    result = trackeditmodecommand:new(...)
  elseif cmd == commandfactory.KEYBOARDMODE then
    result = keyboardmodecommand:new(...)    
  elseif cmd == commandfactory.HZOOMIN then
    result = hzoomincommand:new(...)
  elseif cmd == commandfactory.HZOOMFIT then
    result = hzoomfitcommand:new(...)
  elseif cmd == commandfactory.HZOOMOUT then
    result = hzoomoutcommand:new(...)
  elseif cmd == commandfactory.VZOOMIN then
    result = vzoomincommand:new(...)
  elseif cmd == commandfactory.VZOOMOUT then
    result = vzoomoutcommand:new(...)
  elseif cmd == commandfactory.VZOOMFIT then
    result = vzoomfitcommand:new(...)
  elseif cmd == commandfactory.ZOOMFIT then
    result = zoomfitcommand:new(...)   
  elseif cmd == commandfactory.UNDO then
    result = undocommand:new(...)
  elseif cmd == commandfactory.REDO then
    result = redocommand:new(...)
  end    
  return result
end

return commandfactory
