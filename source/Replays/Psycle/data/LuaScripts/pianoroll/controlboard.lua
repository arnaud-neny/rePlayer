local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local window = require("psycle.ui.window")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local switch = require("switch")
local keymap = require("keymap")

local controlboard = window:new()

controlboard.colors = {
  DEFAULT = 0xFF606060,
  ONTRACK = 0xFFEEEEEE,
  PLAYED = 0xFF1144CC,
  SELECTED = 0xFFFF3322,
  TEXT = 0xFF222222,
  BG = 0xFF333333,
  ROW = 0xFF333333,
  KEY = 0xFF333333,
  OCTAVEROW = 0xFFFF00FF,
  COLOR = 0xFFFFFFFF,
  CMD = 0xFF00FF00
}

controlboard.INSTWK = 121
controlboard.INSMIDI = 123
controlboard.INSTWS = 124
controlboard.INSCMD = 255

controlboard.INSERTTRACKICON = 1
controlboard.REMOVETRACKICON = 2

controlboard.inserttrackiconposition = rect:new(point:new(32, 37), dimension:new(15, 15))
controlboard.removetrackiconposition = rect:new(point:new(32, 0), dimension:new(15, 10))

function controlboard:typename()
  return "controlboard"
end

local cmdpos = {
  [121] = 15,  --TWK
  [123] = 25,  --MIDI  
  [124] = 35,  --TWS  
  [255] = 45 -- CMD
}

function controlboard:new(parent, ...)
  local m = window:new()                
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  if parent ~= nil then
    parent:add(m)
  end  
  return m
end

function controlboard:init(cursor, zoom)
  self.preferredsectionheight_ = 50
  self.track_ = cursor:track()
  self.insertnote = self.INSTWK
  cursor:addlistener(self)
  self.keymap = keymap:new(121, 255)
  self.zoom_ = zoom  
  self:viewdoublebuffered()
  self:addornament(ornamentfactory:createfill(controlboard.colors.KEY))
  self:setautosize(false, false) 
  self:setposition(rect:new(point:new(), dimension:new(45, 0)))
  self.overicon = -1
  self.currsection = -1
end

function controlboard:draw(g)
  for j=1, #self.view.sections do
    local section = self.view.sections[j]
    g:translate(point:new(0, self.view.grid:preferredheight()*(j-1) + self:dy()))
    g:setcolor(controlboard.colors.BG)
    g:fillrect(self:dimension())
    g:setcolor(controlboard.colors.KEY)
    g:fillrect(rect:new(point:new(), dimension:new(self:position():width(), 10))) 
    self:drawsectionheader(g, section)
    self:drawnotes(g)
    self:drawaddandremove(g, j)
    g:retranslate()         
  end
end

function controlboard:drawsectionheader(g, section)
  g:setcolor(controlboard.colors.COLOR)
  g:drawstring(section:label(), point:new())
end  

function controlboard:drawnotes(g)
  self:setbuttoncolor(g, self.insertnote == self.INSTWK)
  g:drawstring("TWK", point:new(0, 10))
  self:setbuttoncolor(g, self.insertnote == self.INSTWS)
  g:drawstring("TWS", point:new(0, 20))
  self:setbuttoncolor(g, self.insertnote == self.INSMIDI)
  g:drawstring("MCM", point:new(0, 30)) 
  self:setbuttoncolor(g, self.insertnote == self.INSCMD)
  g:drawstring("CMD", point:new(0, 40))
end

function controlboard:setbuttoncolor(g, active)
  if active then
    g:setcolor(controlboard.colors.CMD)
  else
    g:setcolor(controlboard.colors.COLOR)
  end
end

function controlboard:drawaddandremove(g, section)
  self:seticonbgcolor(g, section, controlboard.REMOVETRACKICON)
  g:fillrect(controlboard.removetrackiconposition)
  self:seticonbgcolor(g, section, controlboard.INSERTTRACKICON)
  g:fillrect(controlboard.inserttrackiconposition) 
  self:seticoncolor(g, section, controlboard.REMOVETRACKICON)
  g:translate(point:new(5, 0))
  g:drawstring("-", controlboard.removetrackiconposition:topleft())
  self:seticoncolor(g, section, controlboard.INSERTTRACKICON)
  g:drawstring("+", controlboard.inserttrackiconposition:topleft())
  g:retranslate()
end

function controlboard:seticoncolor(g, section, icon)
  if self.currsection == section and self.overicon == icon then
    g:setcolor(controlboard.colors.COLOR)
  else
    g:setcolor(controlboard.colors.COLOR)
  end
end

function controlboard:seticonbgcolor(g, section, icon)
  if self.currsection == section and self.overicon == icon then
    g:setcolor(lighten(controlboard.colors.ROW, 20))
  else
    g:setcolor(0x00000000)
  end
end 
  
function controlboard:note(y)
  local result = 255
  if y < 10 then
    result = 121
  end
  for k, yp in pairs(cmdpos) do    
    if y >= yp and y < yp + self.zoom_:height()then
      result = k      
      break
    end    
  end
  return result
end   

function controlboard:setproperties(properties)  
  if properties.color then
    self.color_ = properties.color:value()
  end
  if properties.keycolor then
    controlboard.colors.KEY = properties.keycolor:value()
    self:removeornaments()
        :addornament(ornamentfactory:createfill(controlboard.colors.KEY))
  end
  if properties.backgroundcolor then
    controlboard.colors.BG = properties.backgroundcolor:value()     
  end
  if properties.selcmdcolor then
    controlboard.colors.CMD = properties.selcmdcolor:value()     
  end
  if properties.ocatvebackgroundcolor then
    self.ocatvebackgroundcolor_ = properties.ocatvebackgroundcolor:value()    
  end
  controlboard.colors.COLOR = properties.color and properties.color:value() or controlboard.colors.COLOR
end

function controlboard:ontrackchanged(cursor)
  self.track_ = cursor:track()
  self:fls()
end

function controlboard:onmousedown(ev)
  local mousepos = self:mapscreentowindow(ev:clientpos())
  local grid = math.floor(mousepos:y() / self.view.grid:preferredheight())
  local yoffset = grid * self.view.grid:preferredheight()
  if mousepos:x() < 30 then
    icon = math.floor((mousepos:y() - yoffset) / 10) - 1
    if icon >= 0 and icon < 4 then
      if icon == 0 then
        self.insertnote = self.INSTWK
      elseif icon == 1 then
        self.insertnote = self.INSTWS
      elseif icon == 2 then
        self.insertnote = self.INSMIDI
      elseif icon == 3 then
        self.insertnote = self.INSCMD
      end
    end
    self:fls()
  else
    if self.overicon == controlboard.INSERTTRACKICON then
      self:parent():oninserttrack(ev, grid + 1)
    elseif self.overicon == controlboard.REMOVETRACKICON  then
      self:parent():onremovetrack(ev, grid + 1)
    end
    self.overicon = -1
    self:updateiconhover(ev)
  end
end

function controlboard:onmousemove(ev)
  self:updateiconhover(ev)
end

function controlboard:onmouseout(ev)
  if self.overicon ~= -1 then
    self.overicon = -1
    self:fls()
  end
end

function controlboard:updateiconhover(ev)
  local mousepos = self:mapscreentowindow(ev:clientpos())
  local grid = math.floor(mousepos:y() / self.view.grid:preferredheight())
  self.currsection = grid + 1
  local yoffset = grid * self.view.grid:preferredheight()
  local oldicon = self.overicon 
  if self:hitinserttrack(point:new(mousepos:x(), mousepos:y() - yoffset)) then    
    self.overicon = controlboard.INSERTTRACKICON   
  elseif self:hitremovetrack(point:new(mousepos:x(), mousepos:y() - yoffset)) then
    self.overicon = controlboard.REMOVETRACKICON
  else
    self.overicon = -1
  end
  if oldicon ~= self.overicon then
    self:fls()
  end
end

function controlboard:hitinserttrack(pos) 
  return controlboard.inserttrackiconposition:intersect(pos)
end

function controlboard:hitremovetrack(pos)
 return controlboard.removetrackiconposition:intersect(pos)
end

function controlboard:mapscreentowindow(screenpos)
  return point:new(screenpos:x() - self:absoluteposition():left(),
                   screenpos:y() - self:dy() - self:absoluteposition():top())
end

function controlboard:dy()
  return self.view:dy()
end

function controlboard:onviewmousemove(key)
end

return controlboard
