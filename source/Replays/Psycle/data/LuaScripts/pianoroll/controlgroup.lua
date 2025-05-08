--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  controlgroup.lua
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
local scrollbar = require("psycle.ui.scrollbar")
local gridsection = require("gridsection")
local controlgridstates = require("controlgridstates")
local controlboard = require("controlboard")
local controlgrid = require("controlgrid")
local gridview = require("gridview")
local tweak = require("psycle.tweak")
local patternevent = require("patternevent")
local sequencebar = require("psycle.sequencebar"):new()
local player = require("psycle.player"):new()

local controlgroup = group:new()

controlgroup.VIEWSECTIONSWMAX = 2

local function makeadd(base, parent, align, aw, ah)
  return base:new(parent):setalign(align):setautosize(aw, ah)
end

function controlgroup:new(parent, ...)
  local m = group:new(parent)
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function controlgroup:init(cursor, zoom, scroller, gridpositions, sequence, playtimer) 
  self.cursor = cursor
  self.zoom_ = zoom
  self.sequence = sequence
  self.playtimer = playtimer
  self.controlboard = controlboard:new(self, cursor, zoom)
                                  :setalign(alignstyle.LEFT)
                                  :setmargin(boxspace:new(0, 3, 0, 1))
  self.controlview = gridview:new(self,
                                  controlgrid:new(),
                                  sequence,
                                  self.controlboard,
                                  scroller,
                                  zoom,
                                  cursor,
                                  gridpositions
                                  )
                             :setalign(alignstyle.CLIENT) 
                             :preventscrolly()
                             :setstates(controlgridstates)
                             :addsizelistener(self)                             
  self.dy_ = 0
  local that = self
  function self.controlview:dy() 
    return that.dy_
  end
  self.controlboard.view = self.controlview                             
  local that = self
  local section = self.controlview.sections[1]
  section:setlabel("Current")
  function section.eventfilter:accept(event)
    return event:track() == that.cursor:track()
  end  
  self:initscroll(self)
  self:inittweak()
end

function controlgroup:initscroll(parent)
   self.scrollbar = makeadd(scrollbar, parent, alignstyle.RIGHT, false, false):setposition(rect:new(point:new(), dimension:new(20, 0)))
   local that = self
   function self.scrollbar:onscroll(scrollbar)
      that.dy_ = -scrollbar:scrollposition()
      that.controlview:fls()
      that.controlboard:fls()
   end
   function self.scrollbar:updatescrollrange()
     self:setscrollrange(0, math.max(0, that.controlview:preferredheight() - that.controlview:position():height())) 
   end
end

function controlgroup:inittweak()
  self.tweak = tweak:new()
  local that = self
  function self.tweak:onautomate(macIdx, param, value, track, line)  
    local seqpos = sequencebar:editposition() + 1
    local pattern = that.sequence:at(seqpos)    
    local pos = line * player:bpt()
    local twk = patternevent:new(124, pos, track, macIdx)
                            :setval16(value)
    pattern:preventnotify():insert(twk, pos):enablenotify()
    that.playtimer:addautomation(twk)
    if not player:playing() then
      local oldpaintmode = that.controlview.paintmode
      that.controlview.paintmode = that.controlview.DRAWAUTOMATION
      that.controlview:fls()
      that.controlview.paintmode = oldpaintmode
    end
  end
  psycle.addtweaklistener(self.tweak)
end

function controlgroup:oninserttrack(ev, grid)  
  local section = gridsection:new(self.controlview, self.cursor:track() .. "")
  section:settrack(self.cursor:track())  
  table.insert(self.controlview.sections, grid + 1, section)
  local track = self.cursor:track()
  function section.eventfilter:accept(event) 
    return event:track() == track
  end
  if #self.controlview.sections <= controlgroup.VIEWSECTIONSWMAX then  
    self:setposition(rect:new(point:new(),
        dimension:new(self:position():width(), self.controlview.grid.preferredsectionheight_ * #self.controlview.sections)))
    self:parent():updatealign()
  end
  self.scrollbar:updatescrollrange()
  self.scrollbar:setscrollposition(self.controlview:sectiontop(grid + 1))
  self.scrollbar:onscroll(self.scrollbar) 
end

 function controlgroup:onremovetrack(ev, grid)
   table.remove(self.controlview.sections, grid)
   if #self.controlview.sections == 1 then
     self:setposition(rect:new(point:new(), dimension:new(self:position():width(), 50)))
     self:parent():updatealign()
   end
   self.scrollbar:updatescrollrange()
   self.scrollbar:setscrollposition(self.controlview:sectiontop(grid))
   self.scrollbar:onscroll(self.scrollbar)
   self:fls()
end

function controlgroup:onsizegridview()
  self.scrollbar:updatescrollrange()
end

function controlgroup:onsize(size)
  self.controlview.grid.preferredsectionheight_ = 
      size:height() / math.min(#self.controlview.sections, controlgroup.VIEWSECTIONSWMAX)
  self.controlboard.preferredsectionheight_ = self.controlview.grid.preferredsectionheight_
  self.controlview.grid:createbackground()
end

return controlgroup
