local selection = {}

function selection:new()
  local m = {}
  setmetatable(m, self)
  self.__index = self  
  return m
end

return selection