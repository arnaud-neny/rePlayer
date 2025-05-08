-- mainviewport.lua

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local keycodes = require("psycle.ui.keycodes")
local group = require("psycle.ui.group")
local viewport = require("psycle.ui.viewport")
local machine = require("psycle.machine")
local scope = require("psycle.ui.scope")

local mainviewport = viewport:new()

function mainviewport:new()
  local m = viewport:new()
  setmetatable(m, self)
  self.__index = self    
  m:init()
  return m
end

function mainviewport:init()    
  self:link(require("psycle.ui.standarduisheet"))
  self:invalidatedirect()
  self:setmindimension(dimension:new(500, 120))
  self:initscopes()  
  self.machines = machine:new()  
end

function mainviewport:initscopes()
  self.scopes = scope:new(self)
                     :setautosize(false, false)
                     :setalign(alignstyle.CLIENT)  
end

return mainviewport
