local player = require("psycle.player"):new()

local dummycommand = {}

function dummycommand:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self
  return m
end

function dummycommand:execute() 
      psycle.output("dummy") 
end

return dummycommand
