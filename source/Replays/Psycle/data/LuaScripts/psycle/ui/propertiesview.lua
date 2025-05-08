-- psycle pluginselector (c) 2016 by psycledelics
-- File: pluginselector.lua
-- copyright 2016 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local node = require("psycle.node")
local signal = require("psycle.signal")
local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local borderradius = require("psycle.ui.borderradius")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local group = require("psycle.ui.group")
local button = require("psycle.ui.button")
local edit = require("psycle.ui.edit")
local text = require("psycle.ui.text")
local checkbox = require("psycle.ui.checkbox")
local combobox = require("psycle.ui.combobox")
local property = require("psycle.property")
local closebutton = require("psycle.ui.closebutton")
local filesave = require("psycle.ui.filesave")
local fileopen = require("psycle.ui.fileopen")
local stock = require("psycle.stock")
local serpent = require("psycle.serpent")
local alignstyle = require("psycle.ui.alignstyle")

local propertiesview = group:new()

function propertiesview:new(parent, name, properties, setting)  
  local c = group:new(parent)  
  setmetatable(c, self)
  self.__index = self
  c:init(name, properties, setting)
  return c
end

function propertiesview:init(name, properties, setting)
  self.edits = {}
  self.doapply = signal:new()
  self.properties_ = properties  
  self.name_ = name  
  self:setautosize(false, false)
  local lineborder = ornamentfactory:createlineborder(0x2F2F2F)
                                    :setborderradius(borderradius:new(5))
  self:addornament(lineborder)
  self:setpadding(boxspace:new(10))
  self:createproperties(name, properties)
  self:applysetting(setting)  
end

function propertiesview:createlabel(parent, label)
  local result = text:new(parent)
                     :setautosize(true, true)
                     :setalign(alignstyle.LEFT)
                     :settext(label)
                     :setmargin(boxspace:new(0, 10, 0, 0))   
  return result                      
end

function propertiesview:createedit(parent, text, width)
  if width == nil then
    width = 150
  end
  local result = edit:new(parent)
                     :setautosize(false, false)
                     :setposition(rect:new(point:new(),
                                           dimension:new(width, 20)))
                     :setalign(alignstyle.LEFT)
                     :settext(text)
                     :setbackgroundcolor(0xFF373533)
                     :setcolor(0xFFFFFFFF)
                     :setmargin(boxspace:new(0, 10, 0, 0))
  return result
end


function propertiesview:createproperties(name, properties)  
  if (properties) then    
    self:preventdraw()
    for name, property in properties:opairs() do      
    if not property:iseditprevented() then        
      local div = group:new(self)
                       :setautosize(false, true)
                       :setalign(alignstyle.TOP)
                       :setposition(rect:new(point:new(), dimension:new(0, 20)))
                       :setmargin(boxspace:new(0, 0, 5, 0))
      if property:typename() == "fontinfo" then
        self:createlabel(div, property:label())
        self[name] = {
          family = self:createedit(div, property:value():family()),            
          size = self:createedit(div, property:value():size()),
          style = self:createedit(div, property:value():style() .. "")                       
      } 
      self.edits[#self.edits + 1] = self[name].family
      self.edits[#self.edits + 1] = self[name].size
      self.edits[#self.edits + 1] = self[name].style
      self:createlabel(div, "[family, size,style]")
    elseif property:typename() == "color" then          
        self:createlabel(div, property:label())
        self[name] = self:createedit(div, property:tostring())
        local stock_color_box = combobox:new(div)
                                        :setautosize(false, false)
                                        :setposition(rect:new(point:new(), dimension:new(100, 20)))
                                        :setalign(alignstyle.LEFT)    
                                        :setmargin(boxspace:new(0, 10, 0, 0))                           
        stock_color_box:additem("Defined Color")
        for k, v in pairs(stock.color) do
          stock_color_box:additem(k.."")      
        end     
        local that = self
        function stock_color_box:onselect(ev)   
         local stockkey = stock.color[self:windows()[self:itemindex()]]
         if stockkey then                   
           that[name]:settext(string.format("%x", stock.value(stockkey)))
           that[name]:setcolor(0xFFCACACA)
           that[name].stockkey = stockkey
         else
           that[name]:setcolor(that.editforegroundcolor)
           that[name].stockkey = false
         end
            end
        if property:stockkey() then                       
          stock_color_box:setitembytext(stock.key(property:stockkey()):match("[^.]+$"))
        that[name].stockkey = property:stockkey()
        end
        self:createlabel(div, "[ARGB HEX]")
      elseif property:typename() == "filepath" then 
        self:createlabel(div, property:label())
        self[name] = self:createedit(div, property:tostring(), 300) 
        local button = button:new(div):settext("Choose" .. property:label()):setautosize(false, false):setalign(alignstyle.LEFT):setposition(rect:new(point:new(), dimension:new(100, 15)))
        local that = self
        function button:onclick()
          local fileopen = fileopen:new()
          fileopen:show()          
          if (fileopen:isok()) then
            that[name]:settext(fileopen:filename())
          end
        end
      elseif property:typename() == "boolean" then         
         self[name] = checkbox:new(div)
                              :setautosize(true, false)
                              :setposition(rect:new(point:new(), dimension:new(100, 15)))
                              :setalign(alignstyle.LEFT)    
                              :settext(property:label())
                              :setchecked(property:value())
                              
      else      
        self:createlabel(div, property:label())
        self[name] = self:createedit(div, property:tostring())                         
      end            
      end 
    end
    self:enabledraw()
  end  
end

function propertiesview:parseproperties()
  if self.properties_ then  
    for name, property in self.properties_:opairs() do    
      if not property:iseditprevented() then
        if property:typename() == "fontinfo" then       
          property:value():setfamily(self[name].family:text())
          property:value():setsize(self[name].size:text())    
        elseif property:typename() == "color" then
          if self[name].stockkey then
          property:setstockkey(self[name].stockkey)
        else
          property:setvalue(tonumber(self[name]:text(), 16))
        end
      elseif property:typename() == "number" then     
        property:setvalue(tonumber(self[name]:text()))  
      elseif property:typename() == "filepath" then
        property:setvalue(self[name]:text())
      elseif property:typename() == "string" then
        property:setvalue(self[name]:text())
      elseif property:typename() == "boolean" then        
        property:setvalue(self[name]:checked())
      end  
    end   
    end
  end
  return self
end

function propertiesview:onapply(name, propertypage, properties)
  self:parseproperties(propertypage, properties)
  self.doapply:emit(name, properties)
end

function propertiesview:applysetting(setting)  
end

return propertiesview