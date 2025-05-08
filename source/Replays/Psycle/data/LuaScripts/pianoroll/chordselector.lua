-- psycle pianoroll (c) 2017 by psycledelics
-- File: chordselector.lua
-- copyright 2017 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local signal = require("psycle.signal")
local node = require("psycle.node")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local image = require("psycle.ui.image")
local images = require("psycle.ui.images")
local alignstyle = require("psycle.ui.alignstyle")
local group = require("psycle.ui.group")
local edit = require("psycle.ui.edit")
local listview = require("psycle.ui.listview")
local text = require("psycle.ui.text")
local closebutton = require("psycle.ui.closebutton")
local toolicon = require("psycle.ui.toolicon")
local scroller = require("scroller")
local keymap = require("keymap")
local keyboard = require("keyboard")
local toolicon = require("psycle.ui.toolicon")
local gridview = require("gridview")
local notegrid = require("notegrid")
local sequence = require("sequence")
local listener = require("listener")
local toolbar = require("psycle.ui.toolbar")
local text = require("psycle.ui.text")
local zoom = require("zoom")
local listview = require("psycle.ui.listview")
local node = require("psycle.node")
local chordselectstates = require("chordselectstates")
local rawpattern = require("psycle.pattern"):new()
local listener = require("psycle.listener")
local cursor = require("trackcursor")
local gridpositions = require("gridpositions")
local chord = require("chord")

local psbdir = require("psycle.config"):new("PatternVisual"):luapath() .. "\\pianoroll\\chords\\"

local chordselector = group:new()


function chordselector:new(parent, ...)  
  local m = group:new(parent)  
  setmetatable(m, self)
  self.__index = self
  m:init(...)
  return m
end

function chordselector:init()
  self:initsequence()
  self.scroller = scroller:new()
  self.zoom_ = zoom:new()
  self.cursor = cursor:new(self.sequence)
  self.gridpositions = gridpositions:new(self.sequence, self.zoom_)
  self.gridgroup = group:new(self)
                        :setdimension(dimension:new(100, 300))
                        :setalign(alignstyle.BOTTOM)
  self.keyboard = keyboard:new(self.gridgroup,
                               keymap:new(16, 44),
                               self.scroller,
                               self.zoom_,
                               self.cursor)
                           :setalign(alignstyle.RIGHT)
                           :setmargin(boxspace:new(0, 3, 0, 1))   
  self.chordview = gridview:new(self.gridgroup,
                                notegrid,
                                self.sequence,
                                self.keyboard,
                                self.scroller,
                                self.zoom_,
                                self.cursor,
                                self.gridpositions)  
                           :setalign(alignstyle.CLIENT) 
                           :setautosize(false, false)                                                  
  self.chordview:setstates(chordselectstates)
  self.chordview.sequence = self.sequence
  function self.chordview:onsize()    
    if self.sequence:at(1) then
      self.zoom_:fithzoom(self.sequence:at(1):numlines(),
          self:position():width())
    end
  end
  self.listeners_ = listener:new()
  self:loadchords()
  self:initlistview()
  self:setdimension(dimension:new(200, 0))
  self:setmargin(boxspace:new(5)) 
end

function chordselector:initlistview()
  self.chordlistview = listview:new(self)
                               :setautosize(false, false)
                               :setalign(alignstyle.CLIENT)                               
  self.chordlistview.statuslisteners_ = listener:new()
  function self.chordlistview:addstatuslistener(listener)
    self.statuslisteners_:addlistener(listener)
  end
  function self.chordlistview:onmouseenter(ev)
    self.statuslisteners_:notify("Select insert chord.", "onstatus")
  end  
  local that = self 
  self.rootnode = node:new()
  local firstnode = nil  
  for i=1, #self.chords do    
    local node = node:new(self.rootnode):settext(self.chords[i]:name())
    node.chord = self.chords[i]
    if not firstnode then
      firstnode = node
    end
  end  
  local that = self
  self.chordlistview:setrootnode(self.rootnode)
  self.chordlistview:selectnode(firstnode)
  function self.chordlistview:onchange(node)
    that.sequence.orderlist[1] = node.chord:pattern()
    that.sequence:notifypatternchange()
    that.gridpositions:update() 
    that.zoom_:fithzoom(node.chord:pattern():numlines(),
        that.chordview:position():width())
    that.chordview:fls()
    that.listeners_:notify(that, "oninsertchordchanged")
  end  
  self.chordlistview:viewlist()
end

function chordselector:initsequence()
  self.defaultchord = chord:new()
                           :setroot()
                           :addlistener(self)
                           :setname("Note")
  self.sequence = sequence:new()   
  self.sequence.orderlist[1] = self.defaultchord:pattern() 
  self.chords = { self.defaultchord }
  return self  
end

function chordselector:loadchords()
  local chordnames = {
    "major", "minor", "sus2", "6", "m6", "7", "major7", "m7", "m-Maj7", "9",
    "major9", "m9", "11", "major11", "m11", "13", "major13", "m13"
  }
  for i=1, #chordnames do
    self:appendchord(chordnames[i])
  end
  return self
end

function chordselector:appendchord(name)
  self.chords[#self.chords + 1] = chord:new(name)
                                       :addlistener(self)
  return self
end

function chordselector:oncalcautodimension()
  return dimension:new(200, 200)
end

function chordselector:chordchanged()  
  self.chordview:fls()
end

function chordselector:chord()
  local result = self.chordlistview:selected()
  if result then
    result = self.chordlistview:selected().chord
  else
    result = self.defaultchord
  end
  return result
end

function chordselector:addlistener(listener)
  self.listeners_:addlistener(listener)
  return self
end

function chordselector:addstatuslistener(listener)
  self.chordlistview:addstatuslistener(listener)
  self.chordview:addstatuslistener(listener)
  return self
end

return chordselector