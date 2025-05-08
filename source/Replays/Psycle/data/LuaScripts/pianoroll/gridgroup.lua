--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  gridgroup.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local keycodes = require("psycle.ui.keycodes")
local group = require("psycle.ui.group")
local toolicon = require("psycle.ui.toolicon")
local gridpositions = require("gridpositions")
local gridview = require("gridview")
local scroller = require("scroller")
local keymap = require("keymap")
local keyboard = require("keyboard")
local notegrid = require("notegrid")
local controlgrid = require("controlgrid")
local rastergrid = require("rastergrid")
local rulergrid = require("rulergrid")
local rulergridstates = require("rulergridstates")
local notegridstates = require("notegridstates")
local controlgroup = require("controlgroup")
local splitter = require("psycle.ui.splitter")
local scrollbar = require("psycle.ui.scrollbar")
local zoom = require("zoom")

local icondir = require("psycle.config"):new("PatternVisual"):luapath() .. "\\psycle\\ui\\icons\\"

local function makeadd(base, parent, align, aw, ah)
  return base:new(parent):setalign(align):setautosize(aw, ah)
end

local gridgroup = group:new()

function gridgroup:new(parent, ...)
  local m = group:new(parent)
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function gridgroup:init(sequence, scroller, keymap, cursor, playtimer)
  self.scroller = scroller
  self.zoom_ = zoom:new()
  self.gridpositions = gridpositions:new(sequence, self.zoom_)
  self.cursor = cursor
  self.cursor:setgridgroup(self)
  self.cursor:addlistener(self.gridpositions)
  self.cursor:addlistener(self)
  self.keyboard = keyboard:new(nil, keymap, scroller, self.zoom_)
                        :setalign(alignstyle.LEFT)
                        :setmargin(boxspace:new(0, 3, 0, 1))
  local topgroup = makeadd(group, self, alignstyle.TOP, false, true)
  self:initkeyboardmodeselect(topgroup)
  self.ruler = gridview:new(topgroup,
                            rulergrid:new(),
                            sequence,
                            self.keyboard,
                            scroller,
                            self.zoom_,
                            cursor,
                            self.gridpositions)
                    :setalign(alignstyle.CLIENT)
                    :setmargin(boxspace:new(0, 0, 2, 49))                    
                    :preventscrolly()
                    :setstates(rulergridstates)
  self.controlgroup = controlgroup:new(self, cursor, self.zoom_, scroller, self.gridpositions, sequence, playtimer)
                                  :setalign(alignstyle.BOTTOM)
                                  :setdimension(dimension:new(0, 50))
  splitter:new(self, splitter.HORZ)                                     
  self:add(self.keyboard)
  self.patternview = gridview:new(self,
                                  notegrid:new(),
                                  sequence,
                                  self.keyboard,
                                  scroller,
                                  self.zoom_,
                                  cursor,
                                  self.gridpositions)
                             :setalign(alignstyle.CLIENT) 
  self.patternview:setstates(notegridstates)
  self.scroller:setview(self.patternview)  
  self:initvgroup() 
  self:inithgroup()  
  self.controlgroup.controlview.patternview = self.patternview  
  self.patternview.controlview = self.controlgroup.controlview
  self.zoom_:addlistener(self.scroller) 
end

function gridgroup:initkeyboardmodeselect(parent)
  self.keyboardmodeselect = toolicon:new(parent)
                                    :settext("Note")                                
                                    :setautosize(false, false)
                                    :setdimension(dimension:new(45, 17)) 
end

function gridgroup:initvgroup()
  local vgroup = makeadd(group, self, alignstyle.RIGHT, true, false)
  self:initvscroll(vgroup) 
  self:initvzoom(vgroup)
end

function gridgroup:inithgroup()
  local hgroup = makeadd(group, self, alignstyle.BOTTOM, false, true)
  self.scroller:addhorizontalinput(scrollbar:new(hgroup, scrollbar.HORZ)
                                            :setalign(alignstyle.CLIENT)
                                            :setautosize(false, false))
  self:inithzoom(hgroup) 
end

function gridgroup:inithzoom(parent)
  local that = self
  self.hzoomfiticon = toolicon:new(parent, icondir .. "fithorizontal.png", 0xFFFFFF):setalign(alignstyle.RIGHT):setautosize(true, false)
  self.hzoomouticon = makeadd(toolicon, parent, alignstyle.RIGHT, true, false):settext("+"):setjustify(toolicon.CENTERJUSTIFY)                                                      
  self.hzoominicon = makeadd(toolicon, parent, alignstyle.RIGHT, true, false):settext("-"):setjustify(toolicon.CENTERJUSTIFY)                                                                          
end

function gridgroup:initvzoom(parent)
  self.zoomfiticon = toolicon:new(parent, icondir .. "fitall.png", 0xFFFFFF):setalign(alignstyle.BOTTOM):setautosize(false, true):setjustify(toolicon.CENTERJUSTIFY)
  self.vzoomfiticon = toolicon:new(parent, icondir .. "fitvertical.png", 0xFFFFFF):setalign(alignstyle.BOTTOM):setautosize(false, true):setjustify(toolicon.CENTERJUSTIFY)
  self.vzoominicon = makeadd(toolicon, parent, alignstyle.BOTTOM, false, true):settext("-"):setjustify(toolicon.CENTERJUSTIFY)
  self.vzoomouticon = makeadd(toolicon, parent, alignstyle.BOTTOM, false, true):settext("+"):setjustify(toolicon.CENTERJUSTIFY)                                                
end

function gridgroup:initvscroll(parent)
  self.scroller:addverticalinput(makeadd(scrollbar, parent, alignstyle.CLIENT, true, false))    
  self.scroller:setdy(-48 * self.zoom_:height())
end

function gridgroup:setchord(chord)
  self.patternview:setchord(chord)
  return self
end

function gridgroup:addstatuslistener(listener)
  self.patternview:addstatuslistener(listener)
  self.ruler:addstatuslistener(listener)
  self.controlgroup.controlview:addstatuslistener(listener)  
  return self
end

function gridgroup:updatedisplayrange()
  self.gridpositions:updatedisplayrange(self.scroller:dx(), self.patternview:position():width())    
end

function gridgroup:toggleseqdisplay()
  self.ruler:toggleseqdisplay()
  self.patternview:toggleseqdisplay()
  self.controlgroup.controlview:toggleseqdisplay()
  self:updatedisplayrange()
end

function gridgroup:zoom()
  return self.zoom_
end

function gridgroup:flscontrolview()
  self.controlgroup.controlview:fls()
end

function gridgroup:ontrackmodechanged()
  self.patternview:fls()
end

return gridgroup

