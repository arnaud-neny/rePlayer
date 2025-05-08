local rawpattern = require("psycle.pattern"):new()

local editquantizecommand = {}

function editquantizecommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function editquantizecommand:init(diff)
  self.diff_ = diff
end

function editquantizecommand:execute()       
    rawpattern:editquantizechange(self.diff_)
end

return editquantizecommand
