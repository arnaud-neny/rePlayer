local player = require("psycle.player"):new()

local insertrowcommand = {}

function insertrowcommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function insertrowcommand:init(sequence, cursor)
  self.sequence_ = sequence
  self.cursor_ = cursor
end

function insertrowcommand:execute()  
  local pattern = self.sequence_:at(self.cursor_:seqpos()) 
  pattern:insertrow(self.cursor_:position() - player:bpt(), self.cursor_:track())
  self.sequence_:notifypatternchange()
  self.cursor_:notify()
end

return insertrowcommand
