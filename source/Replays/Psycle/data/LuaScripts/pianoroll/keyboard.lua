--[[ 
psycle pianoroll (c) 2017 by psycledelics
File:  keyboard.lua
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
local midihelper = require("psycle.midi")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local image = require("psycle.ui.image")
local graphics = require("psycle.ui.graphics")
local player = require("psycle.player")
local zoom = require("zoom")
local cmddef = require("psycle.ui.cmddef")
local patternevent = require("patternevent")


local keyboard = window:new()

keyboard.DISPLAYNOTES = 1
keyboard.DISPLAYDRUMS = 2

keyboard.PLAYTRACK = 1

keyboard.colors = {
  BG = 0xFF333333,
  ROW = 0xFF333333,
  KEY = 0xFF333333,
  OCTAVEKEY = 0xFFFF00FF,
  KEYHIGHLIGHT = 0xFFFF69B4
}

keyboard.displaylabels = {
  [keyboard.DISPLAYNOTES] = "Notes",
  [keyboard.DISPLAYDRUMS] = "Drum"
}

function keyboard:typename()
  return "keyboard"
end

function keyboard:new(parent, ...)  
  local m = window:new()                
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  if parent ~= nil then
    parent:add(m)
  end  
  return m
end

function keyboard:init(keymap, scroller, zoom)
  self.keymap = keymap
  self.scroller = scroller:addlistener(self)
  self.zoom_ = zoom:addlistener(self) 
  self.displaymode = keyboard.DISPLAYNOTES  
  self.player_ = player:new()  
  self.activemap_ = {}  
  self:setautosize(true, false)
  self:setposition(rect:new(point:new(), dimension:new(500, 500)))
  self.mousekey, self.pressedkey_ = -1, -1
  self.color_ = 0xFF0000FF
  self:createkeys(g)
end

function keyboard:transparent()
  return false
end

function keyboard:draw(g)
  g:translate(point:new(0, self.scroller:dy()))
  g:drawimage(self.backgroundimage, point:new())
  self:drawactivemap_(g) 
  self:drawpressedkey(g)
  self:drawmousekey(g)
  self:drawemptyspace(g)  
  g:retranslate()
end

function keyboard:drawmousekey(g)
  if self.mousekey ~= -1 then
    local pos = self:keypos(self.mousekey)
    g:setcolor(keyboard.colors.KEYHIGHLIGHT)   
    g:drawstring(midihelper.notename(self.mousekey - 12), point:new(2, pos + 1))
  end
end

function keyboard:drawemptyspace(g)
  g:setcolor(0xFF232323)
  g:fillrect(rect:new(point:new(0, self:preferredheight()),
                      point:new(self:position():width(), self:position():height() - self.scroller:dy())))
end

function keyboard:toggledisplay()
  self.displaymode = self.displaymode == 1 and self.displaymode + 1 or 1  
  self:createkeys()
  self:fls()
end

function keyboard:displayname()
  return self.displaylabels[self.displaymode]  
end

function keyboard:drawkeys(g)
  for note = self.keymap.range.from_, self.keymap.range.to_ do
    self:drawkey(g, note)
  end
end

function keyboard:drawkeydeviders(g)
  g:setcolor(keyboard.colors.BG)
  for note = self.keymap.range.from_, self.keymap.range.to_ do
    local pos = self:keypos(note)
    g:drawline(point:new(0, pos), point:new(self:position():width(), pos))
  end
end

function keyboard:drawpressedkey(g)
  if self.pressedkey_ ~= -1 then
    self:drawkey(g, self.pressedkey_)
  end
end

function keyboard:drawactivemap_(g)
  for _, entry in pairs(self.activemap_) do   
    self:drawkey(g, entry[1]:note())
  end  
end

function keyboard:keycolors(note)
  local color = 0xFF0000FF
  local textcolor = 0xFF0000FF
  local isoctave = self:isoctave(note)
  local iscurroctave1 = note == 12*cmddef.currentoctave()
  local iscurroctave2 = note == 12*(cmddef.currentoctave() + 1)
  local iscurroctave3 = note == 12*(cmddef.currentoctave() + 2)  
  if isoctave then
    if iscurroctave1 or iscurroctave2 or iscurroctave3 then
      color = lighten(self.colors.OCTAVEKEY, 10)      
    else
      color = self.colors.OCTAVEKEY
    end   
  end
  if isoctave then
    textcolor = 0xFFFF69B4      
  else
    textcolor = self.color_
  end
  if self.activemap_[note] or note == self.pressedkey_ then
    color = self.keyplaycolor_
  end  
  return color, textcolor    
end

function keyboard:drawkeylabel(g, note, pos)
  g:drawstring(self.displaymode == keyboard.DISPLAYNOTES 
               and midihelper.notename(note - 12)
               or midihelper.gmpercussionname(note),
               point:new(2, pos + 1))
end

function keyboard:drawcurroctavelabel(g, note, pos)
  local iscurroctave1 = note == 12*cmddef.currentoctave()
  local iscurroctave2 = note == 12*(cmddef.currentoctave() + 1)
  local iscurroctave3 = note == 12*(cmddef.currentoctave() + 2)  
  local key = -1
  if iscurroctave1 then
    key = cmddef.cmdtokey(cmddef.KEYC_0)
  elseif iscurroctave2 then
    key = cmddef.cmdtokey(cmddef.KEYC_1) 
  elseif iscurroctave3 then
    key = cmddef.cmdtokey(cmddef.KEYC_2)        
  end
  if key ~= -1 then
    g:drawstring(string.char(key), point:new(self:position():width() - 10, pos + 1))
  end
end

function keyboard:drawkey(g, note)
  local pos = self:keypos(note)
  local color, textcolor = self:keycolors(note) 
  if self:isoctave(note) or self.activemap_[note] or self.pressedkey_ ~= -1 then
    g:setcolor(color)
    g:fillrect(rect:new(point:new(0, pos), dimension:new(self:position():width(), self.zoom_:height()-1))) 
  end     
  g:setcolor(textcolor)
  if self.zoom_:height() >= 12 or (note % 12) == 0 or (note % 12) == 7 then    
    self:drawkeylabel(g, note, pos)
  end
  self:drawcurroctavelabel(g, note, pos)
end

function keyboard:isoctave(note)
   return (note % 12) == 0
end

function keyboard:oncalcautodimension()
  return dimension:new(45, 200)
end

function keyboard:preferredheight()
  return self.keymap:numkeys() * self.zoom_:height()
end

function keyboard:setproperties(properties)  
  if properties.color then
    self.color_ = properties.color:value()
  end
  if properties.keycolor then
    keyboard.colors.KEY = properties.keycolor:value() 
  end
  if properties.octavekeycolor then
    keyboard.colors.OCTAVEKEY = properties.octavekeycolor:value() 
  end
  keyboard.colors.KEYHIGHLIGHT = properties.keyhighlightcolor and  properties.keyhighlightcolor:value() or keyboard.colors.KEYHIGHLIGHT 
  keyboard.colors.BG = properties.backgroundcolor and  properties.backgroundcolor:value() or keyboard.colors.BG   
  self.keyplaycolor_ = properties.keyplaycolor and properties.keyplaycolor:value() or 0xFF0000FF
  self:createkeys()
end

function keyboard:onpatterneventheightchanged()
  self:createkeys()
  self:fls()  
end

function keyboard:note(y)
   return math.min(math.max(self.keymap.range.to_ - math.floor(y / self.zoom_:height()),
      self.keymap.range.from_), self.keymap.range.to_)            
end

function keyboard:keypos(key)
  return self.zoom_:height() * (self.keymap.range.to_ - key)
end   

function keyboard:playnote(note)
  if not self.activemap_[note] then
    self.activemap_[note] = { patternevent:new(note, 0) }
    self:fls()
  end
end

function keyboard:stopnote(note)
  if self.activemap_[note] then
    self.activemap_[note] = nil
    self:fls()
  end
end

function keyboard:calcvisiblekeyrange()
    self.visiblenotes = { 
      top = self:note(-self.scroller:dy()),
      bottom = self:note(self:position():height() - self.scroller:dy()),
    }
end

function keyboard:onsize(size) 
  self:calcvisiblekeyrange()
end

function keyboard:onscroll(scroller)
  if scroller:diff():y() ~= 0 then   
    self:calcvisiblekeyrange()
    self:fls()
  end
end

function keyboard:createkeys()
  self.backgroundimage = image:new():reset(
      dimension:new(self:position():width(), self:preferredheight()))
  local g = graphics:new(self.backgroundimage)
  self:clearbackground(g)
  self:drawkeys(g)
  self:drawkeydeviders(g)
  g:dispose()
  return self
end

function keyboard:clearbackground(g)
  g:setcolor(keyboard.colors.KEY)
  g:fillrect(self.backgroundimage:dimension())
end

function keyboard:onplaybarupdate(timer)
  if timer.newnote_ then
    self.activemap_ = timer:activemap()
    self:fls()
  end  
end

function keyboard:onviewmousemove(key)
  if key ~= self.mousekey then
    self.mousekey = key
    self:fls()
  end
end

function keyboard:onmousedown(ev)
  self:capturemouse()
  self.pressedkey_ = self:note(ev:windowpos():y() - self.scroller:dy())
  self.player_:playnote(self.pressedkey_, keyboard.PLAYTRACK)
  self:fls()
end

function keyboard:onmouseup(ev)
  self:releasemouse()
  if self.pressedkey_ ~= -1 then
    self.player_:stopnote(self.pressedkey_, keyboard.PLAYTRACK)
  end
  self.pressedkey_ = -1
  self:fls()
end

return keyboard
