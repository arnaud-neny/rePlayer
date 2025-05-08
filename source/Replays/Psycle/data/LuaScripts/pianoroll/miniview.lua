--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  miniview.lua
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
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local cursorstyle = require("psycle.ui.cursorstyle")
local controlgrid = require("controlgrid")
local rawpattern = require("psycle.pattern")
local sequencebar = require("psycle.sequencebar")
local image = require("psycle.ui.image")
local graphics = require("psycle.ui.graphics")
local gridview = require("gridview")
local scroller = require("scroller")
local sequencebar = require("psycle.sequencebar"):new()

local miniview = window:new()

miniview.DISPLAYALL = 1
miniview.DISPLAYSEQPOS = 2

miniview.displaymode = gridview.DISPLAYALL

miniview.colors = {
  EVENT = 0xFFF0F0F0,
  FONT = 0xFFFFFFFF, 
  BG = 0xFF333333,
  BORDER = 0xFF333333,
  PLAYBAR = 0xFF0000FF, 
  SCROLLBAR = 0xFF666666
}

function miniview:typename()
  return "miniview"
end

function miniview:new(parent, ...)
  local m = window:new()                
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  if parent ~= nil then
    parent:add(m)
  end  
  return m
end

function miniview:init(keymap, sequence)
  self.keymap = keymap
  self.sequence = sequence
  self.sequence:addlistener(self)
  self:viewdoublebuffered()
  self:setautosize(false, false)
  self.sequencebar = sequencebar:new()
  self.evwidth, self.evheight = 2, 1
  self.playbarevwidth = 2
  self.noteheight = self:position():height() / self.keymap:numkeys()
  local that = self
  self.hscroll = { pos = 0, rmin = 0, rmax = 1 }
  function self.hscroll:scrollposition()
    return self.pos
  end
  function self.hscroll:setscrollposition(pos)
    self.pos = pos
    that:fls()
  end
  function self.hscroll:setscrollrange(from, to)
    self.rmin, self.rmax = from, to
  end
  function self.hscroll:scrollrange()
    return self.rmin, self.rmax
  end
  self.vscroll = { pos = 0, rmin = 0, rmax = 1 }
  function self.vscroll:scrollposition()
    return self.pos
  end
  function self.vscroll:setscrollposition(pos)
    self.pos = pos
    that:fls()   
  end
  function self.vscroll:setscrollrange(from, to)
    self.rmin, self.rmax = from, to
  end
  function self.vscroll:scrollrange()
    return self.rmin, self.rmax
  end
  self.updatepatterns_ = {}
  function updatetimer()
    if #self.updatepatterns_ ~= 0 then
      self:updatepatterns()   
    end
  end
  psycle.proxy:setinterval(updatetimer, 200)
end

function miniview:setplaytimer(timer)
   self.playtimer = timer
end

function miniview:toggleseqdisplay()  
  miniview.displaymode = miniview.displaymode == miniview.DISPLAYALL and miniview.DISPLAYSEQPOS or miniview.DISPLAYALL 
  self:onsequenceupdated()
  return self  
end

function miniview:draw(g)
  if self.image then
    g:drawimage(self.image, point:new())  
    self:drawbars(g)
  end
end

function miniview:drawbars(g)
  g:setcolor(self.colors.SCROLLBAR) 
  local hmin, hmax = self.hscroll:scrollrange()
  local left = (self:position():width() - 6 - 20) * self.hscroll:scrollposition() / (hmax - hmin)
  local vmin, vmax = self.vscroll:scrollrange()
  local top = (self:position():height() - 6 - 20) * self.vscroll:scrollposition() / (vmax - vmin) + 10
  g:fillrect(rect:new(point:new(left + 10, self:position():height() - 1), dimension:new(6, 1)))
  g:fillrect(rect:new(point:new(self:position():width() - 1, top), dimension:new(1, 6)))
  if self.playtimer and (self.displaymode == self.DISPLAYALL or self.playtimer:currsequence() == (self.sequencebar:editposition())) then    
    self:drawplaybar(g, self.playtimer:currposition())                
  end
end

function miniview:drawplaybar(g, playposition)
  self:setplaybarcolor(g, playposition)
  self:renderplaybar(g, playposition)
end

function miniview:setplaybarcolor(g, playposition)
  g:setcolor(self.colors.PLAYBAR)
end

function miniview:renderplaybar(g, playposition)
  local x = playposition * self.playbarevwidth + 10
  if self.displaymode == self.DISPLAYALL then
    x = x + self.sequence:startbeat(self.playtimer:currsequence() + 1)
  end
  g:fillrect(rect:new(point:new(x, 0), dimension:new(6, 1)))
end 

function miniview:onmousedown(ev)
  self:scrolltomousepos(ev)
  self.hasdrag = true
  self:setcursor(cursorstyle.POINTER)
  self:capturemouse()
end

function miniview:onmousemove(ev)
   if self.hasdrag then
     self:scrolltomousepos(ev)
  end
end

function miniview:scrolltomousepos(ev)
  local pos = self:mapscreentowindow(ev:clientpos())
  local hmin, hmax = self.hscroll:scrollrange()
  local sx = math.min(math.max(0, pos:x() / (self:position():width() - 6) * (hmax - hmin)), hmax)
  local vmin, vmax = self.vscroll:scrollrange()
  local sy = math.min(math.max(0, pos:y() / (self:position():height() - 6) * (vmax - vmin)), vmax)    
  self.hscroll:setscrollposition(sx)
  self.vscroll:setscrollposition(sy)
  self.hscroll:onscroll(self.hscroll)
  self.vscroll:onscroll(self.vscroll)
end

function miniview:onmouseup(ev)
  self.hasdrag = nil
  self:releasemouse()
  self:setcursor(cursorstyle.DEFAULT)
end

function miniview:mapscreentowindow(screenpos)
  return point:new(screenpos):sub(self:absoluteposition():topleft())
end

function miniview:onsize(dimension)  
  self.noteheight = self:position():height() / self.keymap:numkeys()
  self.evwidth = self:position():width() / self:numbeats()
  self:createpatternimage(dimension)
end

function miniview:onsequenceupdated()
  self.evwidth = self:position():width() / self:numbeats()
  self.playbarevwidth = (self:position():width() - 20) / self:numbeats()
  self:createpatternimage(self:dimension())
  self:fls()
  return self
end

function miniview:numbeats()
  return miniview.displaymode == gridview.DRAWALL 
         and self.sequence:numbeats() 
         or self.sequence:at(sequencebar:editposition() + 1):numbeats()
end         

function miniview:createpatternimage(dimension)
  self.image = image:new():reset(dimension)
  local g = graphics:new(self.image)  
  self:drawbackground(g)  
  g:setcolor(miniview.colors.BORDER)  
  g:drawroundrect(rect:new(point:new(), dimension), dimension:new(10, 10)) 
  g:setcolor(miniview.colors.BG)
  g:drawline(point:new(10, 0), point:new(dimension:width() - 10, 0))
  g:drawline(point:new(10, dimension:height() - 1),
             point:new(dimension:width() - 5, dimension:height() - 1))
  g:drawline(point:new(0, 10),
             point:new(0, dimension:height() - 10))
  g:drawline(point:new(dimension:width() - 1, 10),
             point:new(dimension:width() - 1, dimension:height() - 10))
           
  self:drawevents(g)
  g:dispose()
end

function miniview:drawbackground(g)
  local from, to = self:displayrange()
  local gridpos = 0
  local secondcolor = lighten(miniview.colors.BG, 2)
  local first = true
  for i=from, to do
    local pattern = self.sequence:at(i)
     local width = pattern:numbeats()*self.evwidth
     g:setcolor(first and miniview.colors.BG or secondcolor)
     first = not first
     g:fillrect(rect:new(point:new(gridpos, 0), point:new(gridpos + width, self:position():height())))
     gridpos = gridpos + width
  end
end

function miniview:drawevents(g)
  local gridpos = 0
  g:setcolor(miniview.colors.EVENT)
  local from, to = self:displayrange()  
  for i=from, to do
    local pattern = self.sequence:at(i)
    local curr = pattern:firstnote()
    while curr do
       g:fillrect(rect:new(point:new(gridpos + curr:position()*self.evwidth,
                           self:keypos(curr:note()) + 3),
                           dimension:new(curr:length()*self.evwidth, self.evheight)))                              
       curr = curr.next                                           
     end     
     gridpos = gridpos + pattern:numbeats()*self.evwidth    
  end  
end

function miniview:keypos(key)
  return self.noteheight * (self.keymap.range.to_ - key)
end

function miniview:displayrange()
  local from, to = 1, 0
  if miniview.displaymode == miniview.DISPLAYALL then
    to = self.sequence:len()    
  else
    from = sequencebar:editposition() + 1
    to = from
  end
  return from, to
end

function miniview:onpatternchanged(pattern)
  if not self.image then
    self:onsequenceupdated()
  else
    local found = false
    for i=1, #self.updatepatterns_ do
      if self.updatepatterns_[i] == pattern.ps_ then
        found = true
        break
      end
    end
    if not found then
      self.updatepatterns_[#self.updatepatterns_ + 1] = pattern.ps_
    end
  end  
end

function miniview:updatepatterns()
  if not self.image then
    self:onsequenceupdated()
  else
    local secondcolor = lighten(miniview.colors.BG, 2)
    local g = graphics:new(self.image)
    for k=1, #self.updatepatterns_ do
      local ps = self.updatepatterns_[k]    
      local order = self.sequence.orderlist
      for j=1, #order do
        if order[j].ps_ == ps then 
          local pattern = order[j]       
          g:setcolor((j % 2 ~= 0) and miniview.colors.BG or secondcolor)            
          local gridpos = self.sequence:startbeat(j)*self.evwidth
          local width = pattern:numbeats()*self.evwidth
          g:fillrect(rect:new(point:new(gridpos, 0), point:new(gridpos + width, self:position():height())))
          g:setcolor(miniview.colors.EVENT)
          local curr = pattern:firstnote()
          while curr do
            g:fillrect(rect:new(point:new(gridpos + curr:position()*self.evwidth,
                            self:keypos(curr:note()) + 3),
                            dimension:new(curr:length()*self.evwidth, self.evheight)))                              
            curr = curr.next                                          
          end
        end
      end
    end
    g:dispose()
    self:fls()
  end
  self.updatepatterns_ = {}   
end

function miniview:transparent()
  return false
end

function miniview:setproperties(properties)
  miniview.colors.EVENT = properties.eventcolor and properties.eventcolor:value() or miniview.colors.EVENT
  miniview.colors.BG = properties.backgroundcolor  and properties.backgroundcolor:value() or miniview.colors.BG 
  miniview.colors.PLAYBAR = properties.playbarcolor and properties.playbarcolor:value() or miniview.colors.PLAYBAR
  miniview.colors.SCROLLBAR = properties.scrollbarcolor and properties.scrollbarcolor:value() or miniview.colors.SCROLLBAR
  miniview.colors.BORDER = properties.bordercolor and properties.bordercolor:value() or miniview.colors.BORDER
  self:createpatternimage(self:dimension())
end

function miniview:onplaybarupdate(playtimer) 
  if self.displaymode == miniview.DISPLAYSEQPOS then    
    if playtimer:currsequence() ~= playtimer:prevsequence() then
      self.oldseq = sequencebar:editposition()
      self:onsequenceupdated()
    end
 end
 self:fls()
end

return miniview
