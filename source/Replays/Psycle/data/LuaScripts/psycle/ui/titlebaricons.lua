-- titlebaricons.lua

local cfg = require("psycle.config"):new("PatternVisual")
local signal = require("psycle.signal")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local image = require("psycle.ui.image")
local toolicon = require("psycle.ui.toolicon")
local group = require("psycle.ui.group")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()

local titlebaricons = group:new(parent)

titlebaricons.picdir = cfg:luapath().."\\psycle\\ui\\icons\\"

function titlebaricons:new(parent)
  local m = group:new(parent)
  setmetatable(m, self)
  self.__index = self    
  m:init()
  return m
end

function titlebaricons:init() 
  self:setautosize(false, false)
      :setalign(alignstyle.RIGHT)
  self.windowtoogleicon = toolicon:new(self, titlebaricons.picdir .. "arrow_out.png", 0xFFFFFF)
                                  :setalign(alignstyle.RIGHT)
  self.windowtoogleicon.usetoggleimage_ = true  
  local windowinimg = image:new()
                           :load(titlebaricons.picdir .. "arrow_in.png")
                           :settransparent(0xFFFFFF)                                 
  self.windowtoogleicon:settoggleimage(windowinimg)
  local that = self
  function self.windowtoogleicon:onclick()
    self:resethover()    
    psycle.proxy:toggleviewport()
  end
  self:formattoolicon(self.windowtoogleicon)  
  self.settingsicon = toolicon:new(self, titlebaricons.picdir .. "settings.png", 0xFFFFFF)
                              :setalign(alignstyle.RIGHT)                
  self:formattoolicon(self.settingsicon)    
  local reloadicon = toolicon:new(self, titlebaricons.picdir .. "reload.png", 0xFFFFFF)
                             :setalign(alignstyle.RIGHT)      
                             :setdebugtext("reloadicon")
  function reloadicon:onclick()
    self:resethover()
    psycle.proxy:reload()
  end
  self:formattoolicon(reloadicon)  
  self:setposition(rect:new(point:new(), dimension:new(120, 20)))
end
 
function titlebaricons:formattoolicon(icon)    
  icon:setautosize(false, false)
  icon:setverticalalignment(toolicon.CENTER)
  icon:setjustify(toolicon.CENTERJUSTIFY)
  icon:setposition(rect:new(point:new(), dimension:new(40, 0)))
  return self
end

return titlebaricons