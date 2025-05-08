local eventfilter = {}

function eventfilter:new(...)
  local m = {}
  setmetatable(m, self)
  self.__index = self     
  return m
end

function eventfilter:accept(event)
  return true
end

return eventfilter