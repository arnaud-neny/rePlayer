local player = require("psycle.player"):new()
local sequencebar = require("psycle.sequencebar"):new()
local screenrange = require("screenrange")

local gridpositions = {}

gridpositions.DISPLAYALL = 1
gridpositions.DISPLAYSEQPOS = 2

function gridpositions:new(...)
  local m = {}                
  setmetatable(m, self)
  self.__index = self    
  m:init(...)  
  return m
end

function gridpositions:init(sequence, zoom)
  self.displaymode_ = gridpositions.DISPLAYALL
  self.sequence = sequence
  self.zoom_ = zoom:addlistener(self)
  self.sequence:addlistener(self)
  self.positions = {}
  self.dimensions = {}
  self.displayrange_ = { from = 1, to = 1}
  self.screenranges_ = {}
  self.seqrange = screenrange:new(0, 0, 0)
  self:update()
  self.lastdx = 0
end

function gridpositions:toggledisplaymode()  
  if self.displaymode_ == gridpositions.DISPLAYALL then
    self.displaymode_ = gridpositions.DISPLAYSEQPOS
  else
    self.displaymode_ = gridpositions.DISPLAYALL
  end    
  return self  
end

function gridpositions:displaymode()
  return self.displaymode_
end

function gridpositions:gridwidth()
  local result = 0
  if self.displaymode_ == self.DISPLAYALL then
    local num = self.sequence:len()
    if num > 0 then    
      result = self.positions[#self.positions] + self.dimensions[#self.dimensions]
    end
  else
    result = self:widthat(sequencebar:editposition() + 1)
  end
  return result
end

function gridpositions:at(index)
  return self.positions[index]
end

function gridpositions:widthat(index)
  return self.dimensions[index]
end

function gridpositions:pos(index) 
  return self.displaymode_ == self.DISPLAYALL and self:at(index) or 0
end

function gridpositions:seqposfrom(pos)
  local result = nil
  if self.displaymode_ == gridpositions.DISPLAYALL then
    for i=1, #self.positions do
      if pos >= self.positions[i] and pos < self.positions[i] + self.dimensions[i] then
        result = i
        break      
      end
    end
  else
    result = sequencebar:editposition() + 1
  end
  return result  
end

function gridpositions:onpatterneventwidthchanged()
  self:update()
end

function gridpositions:onpatterneventheightchanged()
  self:update()
end

function gridpositions:onsequenceupdated(sequence)
  self:update()
end

function gridpositions:onpatternchanged()
  self.lastleftevent = nil
end

function gridpositions:update()
  local pos = 0
  self.positions, self.dimensions = {}, {}
  for i=1, self.sequence:len() do
    local pattern = self.sequence:at(i)   
    self.positions[i] = pos    
    self.dimensions[i] = self.zoom_:beatwidth(pattern:numbeats())
    pos = pos + self.dimensions[i]    
  end
  self.lastevent = nil
end

function gridpositions:calcscreenrange(seqpos, dx, width)   
  local result = nil
  local pattern = self.sequence:at(seqpos)  
  if pattern then
    local gridpos = self:pos(seqpos)
    result = screenrange:new(
      seqpos, 
      math.min(pattern:numbeats(), (-gridpos - dx) / self.zoom_:width()),
      math.min(pattern:numbeats(), (-gridpos - dx + width) / self.zoom_:width()))
  end
  return result
end

function gridpositions:updatedisplayrange(dx, width, keeplastevent)
  local seqpos = self:seqposfrom(-dx)
  self.screenranges_ = {}
  self.displayrange_.from, self.displayrange_.to = seqpos and seqpos or 1, 1
  if self.displaymode_ == gridpositions.DISPLAYALL then    
    seqpos = self:seqposfrom(-dx + width)
    self.displayrange_.to = seqpos and seqpos or self.sequence:len()    
  else    
    self.displayrange_.to = self.displayrange_.from
  end
  if self.displaymode_ == gridpositions.DISPLAYALL then 
  self.screenranges_ = {}
    for i = self.displayrange_.from, self.displayrange_.to do
      self.screenranges_[#self.screenranges_ + 1] = self:calcscreenrange(i, dx, width)
    end
  else    
    local pattern = self.sequence:at(seqpos)
    self.seqrange = screenrange:new(
      seqpos,
      math.min(pattern:numbeats(), (-dx) / self.zoom_:width()),
      math.min(pattern:numbeats(), (-dx + width) / self.zoom_:width()))
  end
  if not keeplastevent then
    self.lastleftevent = nil
  end
end

function gridpositions:displayrange()
  return self.displayrange_
end

function gridpositions:screenrange(index)
  local result = nil
  if self.displaymode_ == gridpositions.DISPLAYALL then    
    local i = index - self.displayrange_.from + 1
    result =  i > 0 and self.screenranges_[i] or nil
  else
    result = self.seqrange    
  end  
  return result
end

function gridpositions:firstnote(screenrange, pattern, seqpos)
  return self.lastleftevent 
         and self.lasteventseqpos == seqpos 
         and screenrange:left() >= 0
         and self.lastleftevent
         or pattern:firstnote()   
end

function gridpositions:setlastleftevent(event, seqpos)
  self.lastleftevent = event
  self.lasteventseqpos = seqpos
end

function gridpositions:ontrackchanged()
  self.lastleftevent = nil
end

return gridpositions
