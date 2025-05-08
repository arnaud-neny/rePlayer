local stock = require("psycle.stock")
local serpent = require("psycle.serpent")

local property = {}

function property:new(value, label, typename, stockkey)
  local c = {}
  setmetatable(c, self)
  self.__index = self
  c:init(value, label, typename, stockkey)
  return c
end

function property:clone()
  return property:new(self.value_, self.label_, self.typename_, self.stockkey_)  
end

function property:init(value, label, typename, stockkey)
  self.label_ = ""    
  self.value_ = value  
  self.label_ = label    
  self.stockkey_ = stockkey  
  self.preventedit_ = false
  self.typename_ = typename
end

function property:label()
  return self.label_
end

function property:setvalue(value)
  self.value_ = value
end

function property:value()
  local result
  if self.stockkey_ then
    result = stock.value(self.stockkey_)
  else
    result = self.value_
  end
  return result
end

function property:setstockkey(stockkey)
  self.stockkey_ = stockkey
end

function property:stockkey()
  return self.stockkey_
end

function property:tostring()
  local result, typename = "", self:typename()
  if typename == "filepath" then    
    result = self.value_
  elseif typename == "color" then    
    result = string.format("%x", self.value_)  
  else  
    result = self.value_ .. ""
  end
  return result
end

function property:write(file) 
  file:write('property:new(')  	
  if self:typename() == "fontinfo" then    
    file:write('fontinfo:new(' .. self:value():tostring() .. ')')    
  elseif self:typename() == "color" then		    
    file:write("0x" .. self:tostring())
  elseif self:typename() == "boolean" then
    if self:value() == true then
      file:write("true") 
    else
      file:write("false")
    end		    
  elseif self:typename() == "string" then
	  file:write('"' .. self:tostring() .. '"')
  elseif self:typename() == "filepath" then  
    local path = string.gsub(self:value(), [[\]], [[/]])
    file:write('"' .. path .. '"') 
  elseif self:typename() == "number" then
    file:write(self:tostring())
  end
  if self:typename() == "color" then
    file:write(', "' .. self:label() .. '", "color"')  
  elseif self:typename() == "filepath" then
    file:write(', "' .. self:label() .. '", "filepath"')
  elseif self:typename() == "fontinfo" then
    file:write(', "' .. self:label() .. '", "fontinfo"')
  else
    file:write(', "'.. self:label()..'"')
  end
  if self.stockkey_ then
    file:write(', ' .. stock.key(self.stockkey_))
  end
  file:write(')')
end

function property:typename()
  local result = "unknown"
  if not self.typename_ then
    result = type(self.value_)
  else
    result = self.typename_
  end
  return result
end

function property:enableedit()
  self.preventedit_ = false
  return self
end

function property:preventedit()
  self.preventedit_ = true
  return self
end

function property:iseditprevented()
  return self.preventedit_
end

function property:settypename(typename)
  self.typename_ = typename
end

return property