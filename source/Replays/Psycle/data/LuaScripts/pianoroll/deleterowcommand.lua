local player = require("psycle.player"):new()

local deleterowcommand = {}

function deleterowcommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function deleterowcommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function deleterowcommand:execute()
  if self.cursor_:position() > 0 then  
    local pattern = self.sequence_:at(self.cursor_:seqpos()) 
    local pos = self.cursor_:position() - player:bpt()   
    pattern:deleterow(pos, self.cursor_:track())
    self.sequence_:notifypatternchange()
    self.cursor_:setposition(math.max(0, pos))
  end
end

return deleterowcommand
