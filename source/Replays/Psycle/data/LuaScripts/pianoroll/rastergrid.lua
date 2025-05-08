local player = require("psycle.player")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local image = require("psycle.ui.image")
local graphics = require("psycle.ui.graphics")

local rastergrid = { beatsperbar = 4, bgbeats = 4 }

function rastergrid:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function rastergrid:init()
  self.player = player:new()
end

function rastergrid:setview(view)
  self.view = view
end

function rastergrid:createbackground()  
  self.barimage_ = image:new():reset(self:bardimension())
  local g = graphics:new(self.barimage_)
  self:drawbar(g)
  g:dispose()
end

function rastergrid:drawbar(g)
  if self.barimage_ then
    g:setcolor(self.view.colors.rowcolor)
    g:fillrect(self:bardimension())
    self:drawkeylines(g)     
    self:drawlines(g)   
  end
end

function rastergrid:drawkeylines(g) 
end

function rastergrid:drawlines(g)
  g:setcolor(self.view.colors.rastercolor) 
  local ticks = self.player:tpb()
  local p1, p2 = point:new(), point:new(0, self:preferredheight())
  for i=0, ticks * self.bgbeats do    
    local x = i * self.view:zoom():width() / ticks  
    g:drawline(p1:setx(x), p2:setx(x))     
  end
end

function rastergrid:drawbars(g, screenrange, pattern)
  self:drawimages(g, screenrange, pattern)
  self:drawbeatlines(g, screenrange)
  self:drawbarendings(g, screenrange)
  self:drawpatternend(g, self.view:zoom():beatwidth(pattern:numbeats())) 
end

function rastergrid:drawimages(g, screenrange, pattern)
  if self.barimage_ then
    local bgwidth = math.floor(self.bgbeats * self.view:zoom():width())
    local numbgs = pattern:numbeats() / self.bgbeats 
    local from = math.max(0, math.floor(screenrange:left() / self.bgbeats))
    local to = math.min(numbgs, math.ceil(screenrange:right() / self.bgbeats)) 
    for i=from, to do
      g:drawimage(self.barimage_, point:new(i*bgwidth, 0))    
    end
  end
end

function rastergrid:drawbeatlines(g, screenrange)
  g:setcolor(self.view.colors.linebeatcolor)
  local p1, p2 = point:new(), point:new(0, self:preferredheight())
  for i=0, screenrange:right() do
    local x = i * self.view:zoom():width()
    g:drawline(p1:setx(x), p2:setx(x))      
  end
end

function rastergrid:drawbarendings(g, screenrange)
  local h = self:bardimension():height()
  g:setcolor(self.view.colors.linebarcolor)
  local p1, p2 = point:new(), point:new(0, self:preferredheight())
  for i=0, screenrange:right(), self.beatsperbar do 
    local x = i * self.view:zoom():width()
    g:drawline(p1:setx(x), p2:setx(x))
  end
end

function rastergrid:drawpatternend(g, patternwidth, height)
  g:setcolor(self.view.colors.patternendcolor)  
  g:fillrect(rect:new(point:new(patternwidth - 5, 0), dimension:new(5, self:bardimension():height())))
end

function rastergrid:bardimension()
   return dimension:new(math.floor(self.beatsperbar * self.view:zoom():width()), self:preferredheight())
end

function rastergrid:drawcursorbar(g, seqpos) 
  if self.view.cursor:seqpos() == seqpos then
    g:setcolor(self.view.colors.cursorbarcolor)
    self:rendercursorbar(g, self.view.cursor:position())   
  end             
end

function rastergrid:hittestselrect()
  return {}
end

function rastergrid:seteventcolor(g, event, seltrack, isplayed)   
  local isselectedtrack = event:track() == seltrack 
  local color = self.view.colors.eventdarkcolor 
  if isselectedtrack then             
   color = self.view.colors.eventcolor 
  end      
  if event:selected() then
   if self.view.selectionrect ~= nil then
     color = self.view.colors.eventblockcolor
   else
     color = self.view.colors.seleventcolor
   end
  end       
  if isplayed then              
   color = self.view.colors.eventplaycolor
  end
  g:setcolor(color)
end

function rastergrid:clearplaybar(g, playposition)
end

function rastergrid:clearbackground(g, screenrange, seqpos)
  if self.view.paintmode == self.view.DRAWALL then
    self:drawbars(g, screenrange, self.view.sequence:at(seqpos))  
  end
end

function rastergrid:drawplaybar(g, playposition) 
  g:setcolor(self.view.colors.playbarcolor)
  self:renderplaybar(g, playposition)             
end

function rastergrid:renderplaybar(g, playposition)
  local x = playposition * self.view:zoom():width()
  g:drawline(point:new(x, - self.view:dy()), 
             point:new(x, self.view:position():height() - self.view:dy()))
end     

function rastergrid:rendercursorbar(g, position)
  local xpos = self.view:zoom():beatwidth(position)
  g:drawline(point:new(xpos, -self.view:dy()),
             point:new(xpos, self.view:position():height() - self.view:dy()))    
end        

function rastergrid:setclearcolor(g, playposition)
  if playposition % 1 == 0 then
    if playposition % self.player:tpb() == 0 then 
      g:setcolor(self.view.colors.line4beatcolor)
    else
      g:setcolor(self.view.colors.linebeatcolor)
    end
  else
    g:setcolor(self.view.colors.rastercolor)
  end
end

return rastergrid
