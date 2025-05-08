--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  gridview.lua
copyright 2017 members of the psycle project http://psycle.sourceforge.net
This source is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation ; either version 2, or (at your option) any later version.
]]

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local window = require("psycle.ui.window")
local sequencebar = require("psycle.sequencebar"):new()
local statehandler = require("statehandler")
local drag = require("drag")
local player = require("psycle.player"):new()
local keycodes = require("psycle.ui.keycodes")
local playbarrender = require("playbarrender")
local listeners = require("psycle.listener")
local gridpositions = require("gridpositions")
local gridsection = require("gridsection")
local signal = require("psycle.signal")
local eventposition = require("eventposition")
local region = require("psycle.ui.region")

local roundedge = dimension:new(5, 5)

local gridview = window:new()

gridview.colorkeys = {
  "event", "eventplay", "selevent", "font", "fontevent", "fonteventplay",
  "row", "background", "octaverow", "linebeat", "line4beat", "selbackground",
  "cursorbar", "linebar", "raster","patternend", "playbar", "gridborder",
  "eventblock", "sliceline"
}

gridview.DRAWALL = 1
gridview.DRAWEVENTS = 2
gridview.DRAWAUTOMATION = 3

gridview.EDITMODE = 1
gridview.SELECTMODE = 2
gridview.SLICEMODE = 3

gridview.COPYMODE = 3
gridview.MOVEMODE = 4

gridview.DISPLAYALL = 1
gridview.DISPLAYSEQPOS = 2

gridview.displaymode = gridview.DISPLAYALL

function gridview.setplaytimer(timer)
 gridview.playtimer = timer
end

function gridview:typename()
  return "gridview"
end

function gridview:new(parent, ...)
  local m = window:new()                
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  if parent ~= nil then
    parent:add(m)
  end  
  return m
end

function gridview:init(grid, sequence, keyboard, scroller, zoom, cursor, gridpositions)
  self.sequence = sequence
  self.sections = {}
  self.sections[1] = gridsection:new(self)
  self.selectionsection = 1
  self.gridclass = gridclass 
  self.paintmode = gridview.DRAWALL
  self.drag = drag:new(self.sequence)
  self.keyboard = keyboard  
  self.hasscrolly_ = true
  self.scroller = scroller
  self.cursor = cursor
  self.scroller:addlistener(self)  
  self.grid = grid
  self.grid:setview(self)
  self.inputmode = gridview.EDITMODE
  self.dragmode = gridview.MOVEMODE  
  self.selectionrect = nil
  self.zoom_ = zoom:addlistener(self) 
  self.gridpositions_ = gridpositions  
  self:viewdoublebuffered()
  self.laststopoffset_ = 0.75
  self.statehandler = statehandler:new(self)
  self.sizelisteners_ = listeners:new()
  self.oldseq = 1
  self.mousepos_ = point:new()
  self.cursor:addlistener(self)
  self.chord_ = nil
  self.player_ = player
  self.timeridscrollupdown = -1
  self.timeridscrollleftright = -1
  self.autoscroll_ = false  
end

function gridview:addstatuslistener(listener)
  self.statehandler:addstatuslistener(listener)
  return self
end

function gridview:gridpositions()
  return self.gridpositions_
end

function gridview:addsizelistener(listener)
  self.sizelisteners_:addlistener(listener)
  return self
end

function gridview:setstates(states)
  self.states = states
  self.statehandler:setstate(self.states.idling, self)
  return self
end 

function gridview:toggleseqdisplay()  
  gridview.displaymode = gridview.displaymode == gridview.DISPLAYALL and gridview.DISPLAYSEQPOS or gridview.DISPLAYALL 
  self.gridpositions_:toggledisplaymode()  
  return self  
end

function gridview.showsequence()
  gridview.displaymode = gridview.DISPLAYALL
  return self  
end

function gridview.showcurrsequence()
  gridview.displaymode = gridview.DISPLAYSEQPOS
  return self  
end

function gridview:preventscrolly()
  self.hasscrolly_ = false
  return self
end

function gridview:transparent()
  return false
end

function gridview:setcommands(editcommands)
  self.editcommands_ = editcommands
end

function gridview:setinputmode(mode)
  self.inputmode = mode
  if mode == gridview.EDITMODE then
    self.statehandler:setstate(self.states.idling, self)
  elseif mode == gridview.SELECTMODE then
    self.statehandler:setstate(self.states.selectidling, self)
  elseif mode == gridview.SLICEMODE then
    self.statehandler:setstate(self.states.slicing, self)
  end
end

function gridview:setdragmode(mode)
  self.dragmode = mode  
end

function gridview:toggletrackviewmode()
  self.sequence:toggletrackviewmode()
  self:fls()
end

function gridview:draw(g, region)
  local sections = { from = 1, to = 1 }
  if #self.sections > 1 then
    sections = {
      from = math.max(1, self:sectionindex(region:bounds():top() - self:dy())),
      to = math.max(1, math.min(#self.sections, self:sectionindex(region:bounds():bottom() - self:dy() - 1)))
    }
  end
  g:translate(point:new(self.scroller:dx(), self:dy()))  
  self:clearplaybar(g)      
  self:drawgrids(g, sections)
  self:drawslice(g)
  g:retranslate()
  self:drawemptyspace(g)
end

function gridview:dy()
  return self.hasscrolly_ and self.scroller:dy() or 0
end

function gridview:drawgrids(g, sections)  
  for j=sections.from, sections.to do
    g:translate(point:new(0, self.grid:preferredheight()*(j-1)))
    if not self.selectionrect then
      self:drawdefaultgrids(g, j)
    else
      self:drawselgrids(g, j)
    end
    self:drawgridend(g)
    g:retranslate()
  end   
end

function gridview:drawgridend(g)
  g:setcolor(gridview.colors.gridbordercolor)
   :translate(point:new(-self.scroller:dx(), self.grid:preferredheight() - 1))  
   :drawline(point:new(), point:new(self:position():width(), 0))
   :retranslate()
end

function gridview:drawdefaultgrids(g, j)
  for i= self.gridpositions_:displayrange().from, self.gridpositions_:displayrange().to do 
    local screenrange = self.gridpositions_:screenrange(i)
    if screenrange and screenrange:right() >= 0 and screenrange:left() ~= screenrange:right() then
      g:translate(point:new(self.gridpositions_:pos(i), 0))     
      self.grid:clearbackground(g, screenrange, i, self.sections[j])      
      self.grid:draw(g, screenrange, self.sequence:at(i), i, self.sections[j])                
      if player:playing() and self.playtimer:currsequence() == (i - 1) then       
        self.grid:drawplaybar(g, self.playtimer:currposition())                
      end       
      g:retranslate()
    end
  end     
end

function gridview:drawselgrids(g, j)  
  if gridview.DRAWALL then 
    for i = self.gridpositions_:displayrange().from, self.gridpositions_:displayrange().to do
      local screenrange = self.gridpositions_:screenrange(i)
      if screenrange and screenrange:onscreen() then                     
        g:translate(point:new(self.gridpositions_:pos(i), 0))             
        self.grid:drawbars(g, screenrange, self.sequence:at(i))        
        g:retranslate()
      end
    end
  end      
  self:drawselectionrect(g, j)
  for i = self.gridpositions_:displayrange().from, self.gridpositions_:displayrange().to do 
    local screenrange = self.gridpositions_:screenrange(i)
    if screenrange and screenrange:onscreen() then 
      g:translate(point:new(self.gridpositions_:pos(i), 0))      
      self.grid:draw(g, screenrange, self.sequence:at(i), i, self.sections[j])
      if self.playtimer:currsequence() == (i - 1) then       
        self.grid:drawplaybar(g, self.playtimer:currposition())                
      end
      g:retranslate()
    end
  end
end

function gridview:drawselectionrect(g, section)
  if self.selectionrect and section == self.selectionsection then      
    g:setcolor(gridview.colors.selbackgroundcolor)      
     :fillroundrect(self.selectionrect, roundedge)
     :setcolor(gridview.colors.fontcolor)    
     :drawroundrect(self.selectionrect, roundedge)    
  end
end

function gridview:drawslice(g)
  if self.slicestart then
    g:setcolor(gridview.colors.slicelinecolor)
    g:drawline(point:new(self.zoom_:beatwidth(self.slicestart:position()),
                         self.keyboard:keypos(self.slicestart:note())),
               point:new(self.zoom_:beatwidth(self.sliceend:position()),
                         self.keyboard:keypos(self.sliceend:note())))
  end  
end

function gridview:drawemptyspace(g)   
  g:setcolor(gridview.colors.backgroundcolor)
  local width = self:position():width() - self.scroller:dx() - self:preferredgridwidth()
  if width > 0 then
    g:fillrect(rect:new(point:new(self:position():width() - width, 0),
                        dimension:new(self:position():width(), self:position():height())))
  end
  local height = self:position():height() - self:dy() - self:preferredheight()  
  if height > 0 then
    g:fillrect(rect:new(point:new(0, self:position():height() - height),
                        dimension:new(self:position():width(), height)))  
  end                         
end

function gridview:onpatterneventwidthchanged()
  self.gridpositions_:updatedisplayrange(self.scroller:dx(), self:position():width())
  self.grid:createbackground()
  self:fls()
end

function gridview:onpatterneventheightchanged()
  self.grid:createbackground()
  self.paintmode = gridview.DRAWALL
  self:fls()
end

function gridview:onscroll(scroller)
  if scroller:diff():x() ~= 0 then
    self.gridpositions_:updatedisplayrange(self.scroller:dx(), self:position():width(), scroller:diff():x() <= 0)
  else
    self.gridpositions_.lastleftevent = nil
  end
  self:fls()
end

function gridview:onsize(size) 
  self.sizelisteners_:notify(self, "onsizegridview")
  self.gridpositions_:updatedisplayrange(self.scroller:dx(), self:position():width())
end

function gridview:onplaybarupdate(playtimer)   
  if sequencebar:followsong() and player:playing() then
    self.cursor:preventnotify()
               :setseqpos(playtimer:currsequence() + 1)
               :setposition(playtimer:currposition())
               :enablenotify()
  end
  if self.displaymode == gridview.DISPLAYSEQPOS then    
    if self.oldseq ~= sequencebar:editposition() then
      self.oldseq = sequencebar:editposition()
      self.paintmode = gridview.DRAWALL
      self.gridpositions_:updatedisplayrange(self.scroller:dx(), self:position():width())
    else     
      self.paintmode = gridview.DRAWEVENTS  
    end
    self:fls()
    self.paintmode = gridview.DRAWALL 
  else
    local playseq = playtimer:currsequence() + 1
    if playseq >= self.gridpositions_:displayrange().from and playseq <= self.gridpositions_:displayrange().to or playseq ~= playtimer:prevsequence() + 1 then
      self.paintmode = gridview.DRAWEVENTS    
      self:fls()
      self.paintmode = gridview.DRAWALL  
    end
  end
end

function gridview:setproperties(properties)
  self:copycolors(properties)
  self:computecolors()
  self.grid:createbackground()  
end

function gridview:copycolors(properties)
  gridview.colors = {}
  for k=1, #gridview.colorkeys do
    local key = gridview.colorkeys[k] .. "color"   
    gridview.colors[key] = 
       properties[key] and properties[key]:value()  or 0xFFFFFFFF  
  end  
end

function gridview:computecolors()
  gridview.colors.eventresizecolor = lighten(gridview.colors.eventcolor, 20)
  gridview.colors.eventdarkcolor = lighten(gridview.colors.eventcolor, -64)
end

function gridview:hittest(position)
  local pos = position
              and eventposition:new(
                  self:beat(position:x()),
                  self.keyboard:note(position:y()),
                  self:seqposfrom(position:x()))
               or self:eventmousepos()
  position = position and position or self:mousepos()                
  local result = nil
  if self.sequence and pos:seqpos() then
    local sectionindex = position 
                         and self:sectionindex(position:y()) 
                         or self:sectionindex(self:mousepos():y())
    result = self.grid:hittest(pos, self.sequence:at(pos:seqpos()), sectionindex)
    result.section = self.sections[sectionindex]
    result.sectionoffset = self:sectiontop(sectionindex)        
  end
  return result
end

function gridview:sectionindex(y)
  return math.min(math.floor(y / self.grid:preferredheight()) + 1, #self.sections)
end

function gridview:sectiontop(index)
  return (index - 1) * self.grid:preferredheight()    
end

function gridview:hittestselrect()   
  return self.grid:hittestselrect()
end

function gridview:seqposfrom(posx)
  result = nil
  if gridview.displaymode == gridview.DISPLAYALL then
    result = self:gridpositions():seqposfrom(posx)
  else    
    result = sequencebar:editposition() + 1
  end
  return result
end

function gridview:keyfrom(yposition)
  return self.keyboard:note(yposition)
end 

function gridview:visiblenotes()
  local dy = self:dy()  
  local that = self
  return { 
    top = that.keyboard:note(-dy),
    bottom = that.keyboard:note(that:position():height() - that:zoom():height() - dy),
  } 
end

function gridview:deselectall()  
  self.sequence:deselectall()
  self:fls()  
end

function gridview:preferredgridwidth()
  return gridview.displaymode == gridview.DRAWALL 
         and self:gridpositions():gridwidth() 
         or self:gridpositions():widthat(sequencebar:editposition() + 1)  
end

function gridview:preferredheight()
  return #self.sections * self.grid:preferredheight()
end

function gridview:ontrackchanged()
  self:fls()
end

function gridview:onsequenceupdated()
  self.grid:createbackground()
  self.gridpositions_:updatedisplayrange(self.scroller:dx(), self:position():width())
  self:fls()
end

function gridview:clearplaybar(g)
  if self.paintmode == gridview.DRAWEVENTS then
    local seqpos = self.playtimer:prevsequence() + 1
    local screenrange = self.gridpositions_:screenrange(seqpos) 
    if screenrange and screenrange:haspos(self.playtimer:prevposition()) then
      g:translate(point:new(self:gridpositions():pos(seqpos), 0))         
      local playposition = self.playtimer:prevposition()
      if sequencebar:followsong() and player:playing() then
        g:setcolor(self.colors.rowcolor)
        self.grid:rendercursorbar(g, playposition)
      end  
      self.grid:setclearcolor(g, playposition)          
      self.grid:renderplaybar(g, playposition)
      self.grid:clearplaybar(g, playposition)
      g:retranslate()
    end   
  end 
end

function gridview:onmouseenter(ev)
  self:updatemouse(ev)  
  self.statehandler:onmouseenter(self)
end

function gridview:onmousedown(ev)
  self:updatemouse(ev)
  self.statehandler:transition("handlemousedown", self, ev:button())   
end

function gridview:onmousemove(ev)
  local oldnote = self:eventmousepos():note()
  local oldposition = self:eventmousepos():rasterposition()
  self:updatemouse(ev)
  self.keyboard:onviewmousemove(self:eventmousepos():note())
  self.statehandler:transition("handlemousemove", self, ev:button())
  ev:stoppropagation()
end

function gridview:onmouseup(ev)
  self:updatemouse(ev)
  self.statehandler:transition("handlemouseup", self, ev:button()) 
end

function gridview:updatemouse(ev)
  self.mousepos_ = ev:clientpos()
  self:doautoscroll()
end

function gridview:onautoscroll()
  self.statehandler:transition("handleautoscroll", self)
end

function gridview:mapscreentowindow(pos)
  return point:new(pos:x() - self.scroller:dx() - self:absoluteposition():left(),
                   pos:y() - self:dy() - self:absoluteposition():top())
end

function gridview:mousepos()
  return self:mapscreentowindow(self.mousepos_)
end

function gridview:beat(x)
  local result = 0
  local seqpos = self:seqposfrom(x)                                                   
  if seqpos then
    result = self.zoom_:beat(x - self.gridpositions_:pos(seqpos))
  end
  return result, seqpos
end

function gridview:startdrag(...)
  self.drag:start(self, ...)
  return self
end

function gridview:stopdrag()  
  if self.dragmode == self.MOVEMODE then
    self.drag:erase()
             :insert(self)             
  else
    self.drag:copy(self)
  end
  self:adjustcursor(true)
  self.drag:stop(self)
  return self
end

function gridview:adjustcursor(keepselection)
  self.cursor:setseqpos(self:seqposfrom(self:mousepos():x()))
             :setposition(self:beat(self:mousepos():x()), keepselection)
end

function gridview:eventmousepos()
  local pos = self:mapscreentowindow(self.mousepos_)  
  return eventposition:new(
    self:beat(pos:x()),
    self.keyboard:note(pos:y()),
    self:seqposfrom(pos:x()))
end

function gridview:doautoscroll()
  if self:autoscroll() and self:eventmousepos():seqpos() then
    if self:eventmousepos():seqpos() and self:eventmousepos():note() > self:visiblenotes().top then      
      self:startscrollup()      
    elseif self:eventmousepos():note() < self:visiblenotes().bottom then
      self:startscrolldown()
    else
      self:fls()
    end
    local screenrange = self.gridpositions_:screenrange(self:eventmousepos():seqpos())
    if screenrange and self:eventmousepos():rasterposition() + player:bpt() > screenrange:right() then
      self:startscrollright()
    elseif screenrange and self:eventmousepos():rasterposition() - player:bpt() < screenrange:left() then
      self:startscrollleft()
    else
      self:fls()
    end    
  end
end

function gridview:abortdrag(view)
  self.drag:abort(view)
  self:fls()
  return self
end

function gridview:setchord(chord)
  self.chord_ = chord
end

function gridview:chord()
  return self.chord_
end

function gridview:screenrange(index)
  local result = self.gridpositions_:screenrange(index)
  if not result then
    result = self.gridpositions_:calcscreenrange(index, self.scroller:dx(), self:position():width())
  end
  return result
end

function gridview:setlaststop(stop)
  self.laststopoffset_ = stop
end

function gridview:startslice()
  self.slicestart = self:eventmousepos():clone()
  self:updateslice()
end

function gridview:updateslice()
  self.sliceend = self:eventmousepos():clone()
  self:fls()
end

function gridview:stopslice()
  if self.slicestart then
    local slicepositions = {}
    local startnote = math.min(self.slicestart:note(), self.sliceend:note())
    local endnote = math.max(self.slicestart:note(), self.sliceend:note()) 
    if self:raster(self.slicestart:position()) == self:raster(self.sliceend:position()) then
      for i=startnote, endnote do
        slicepositions[i] = self:raster(self.slicestart:position())
      end
    else
      local d = (self.slicestart:position() - self.sliceend:position())/(startnote - endnote)
      for i=startnote, endnote do
        slicepositions[i] = self:raster(d*(endnote-i) + self.slicestart:position())
      end    
    end  
    local curr = self:pattern():firstnote()
    while curr do
      local slice = slicepositions[curr:note()]
      if slice and slice > curr:position() and slice < curr:position() + curr:length() then
        self:pattern():insert(curr:clone(), slice)
      end
      curr = curr.next
    end  
    self.slicestart, self.sliceend = nil, nil
  end
end

function gridview:raster(pos)
  return math.floor(pos*player:tpb())/player:tpb()
end

function gridview:startscrollup()
  if self.timeridscrollupdown == -1 then    
    function scrollup()  
      if self.timeridscrollupdown ~= - 1 then
       if self:eventmousepos():note() > self:visiblenotes().top then
          self.scroller:setdy(self.scroller:dy() + self.zoom_:height())                    
          self.timeridscrollupdown = psycle.proxy:settimeout(scrollup, 45)
          self:onautoscroll()          
        else
          self.timeridscrollupdown = -1
        end
      else
        self.timeridscrollupdown = -1
      end    
    end  
    self.timeridscrollupdown = psycle.proxy:settimeout(scrollup, 0)     
  end
end

function gridview:startscrolldown()
  if self.timeridscrollupdown == -1 then 
    function scrolldown()
      if self.timeridscrollupdown ~= - 1 then      
        if self:eventmousepos():note() < self:visiblenotes().bottom then       
          self.scroller:setdy(self.scroller:dy() - self.zoom_:height())                    
          self.timeridscrollupdown = psycle.proxy:settimeout(scrolldown, 45)
          self:onautoscroll()
        else
          self.timeridscrollupdown = -1
        end
      else
        self.timeridscrollupdown = -1
      end
    end
    self.timeridscrollupdown = psycle.proxy:settimeout(scrolldown, 0)    
  end  
end

function gridview:startscrollleft()
  if self.timeridscrollleftright == -1 then
    function scrollleft()
      if self:eventmousepos():seqpos() and self.timeridscrollleftright ~= - 1 then
        local screenrange = self.gridpositions_:screenrange(self:eventmousepos():seqpos())
        if screenrange and self:pattern() and self:eventmousepos():rasterposition() > 0
           and self:eventmousepos():rasterposition() - player:bpt() < screenrange:left()
        then           
          self.scroller:setdx(self.scroller:dx() + self.zoom_:rasterwidth())  
          self.timeridscrollleftright = psycle.proxy:settimeout(scrollleft, 45)
          self:onautoscroll()
        else
          self.timeridscrollleftright = -1
        end      
      else
        self.timeridscrollleftright = -1
      end
    end      
    self.timeridscrollleftright = psycle.proxy:settimeout(scrollleft, 0)    
  end
end

function gridview:startscrollright()
  if self.timeridscrollleftright == -1 then
    function scrollright()
      if self:eventmousepos():seqpos() and self.timeridscrollleftright ~= - 1 then
        local screenrange = self.gridpositions_:screenrange(self:eventmousepos():seqpos())
        if screenrange and self:pattern() and self:eventmousepos():rasterposition() < self:pattern():numbeats()
           and self:eventmousepos():rasterposition() + player:bpt() > screenrange:right()
        then        
          self.scroller:setdx(self.scroller:dx() - self.zoom_:rasterwidth())      
          self.timeridscrollleftright = psycle.proxy:settimeout(scrollright, 45)
          self:onautoscroll()
        else
          self.timeridscrollleftright = -1
        end      
      else
        self.timeridscrollleftright = -1
      end
    end
    self.timeridscrollleftright = psycle.proxy:settimeout(scrollright, 0)
  end
end

function gridview:enableautoscroll()
  self.autoscroll_ = true
end

function gridview:preventautoscroll()
  self.autoscroll_ = false
end

function gridview:autoscroll()
  return self.autoscroll_
end

function gridview:pattern()
  return self.sequence:at(self:eventmousepos():seqpos())
end

function gridview:redrawsection(index)
  local pos = self:sectionposition(index)    
  self:fls(region:new():setrect(pos))
end

function gridview:sectionposition(index)
  return rect:new(point:new(0, self:sectiontop(index) + self:dy()),
                  dimension:new(self:position():width(),
                                self.grid:preferredheight()))
end

function gridview:zoom()
  return self.zoom_
end

return gridview
