-- psycle plugineditor (c) 2015, 2016 by psycledelics
-- File: search.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local keycodes = require("psycle.ui.keycodes")
local alignstyle = require("psycle.ui.alignstyle")
local signal = require("psycle.signal")
local point = require("psycle.ui.point")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local group = require("psycle.ui.group")
local button = require("psycle.ui.button")
local edit = require("psycle.ui.edit")
local text = require("psycle.ui.text")
local checkbox = require("psycle.ui.checkbox")
local iconbutton = require("psycle.ui.toolicon")
local closebutton = require("psycle.ui.closebutton")
local search = group:new()

search.DOWN = 1
search.UP = 2                            

function search:new(parent)
  local c = group:new(parent)  
  setmetatable(c, self)
  self.__index = self  
  c:init()  
  return c
end

function search:init()  
  self:setautosize(false, true)
  self:setpadding(boxspace:new(5))  
  self.dosearch = signal:new()
  self.dohide = signal:new()
  self.doreplace = signal:new()
  local g = group:new(self):setautosize(true, true):setalign(alignstyle.RIGHT)
  local closebutton = closebutton.new(g):setalign(alignstyle.TOP)  
  local that = self
  function closebutton:onclick()  
    that.dohide:emit()
  end
  self:createeditgroup(self)  
  self:createreplacegroup(self)  
end

function search:createeditgroup(parent)       
 self.editgroup = group:new(parent)
                       :setautosize(true, true):setalign(alignstyle.LEFT) 
 local optionrow = group:new(self.editgroup)
                        :setautosize(true, true)
						            :setalign(alignstyle.TOP)
						            :setmargin(boxspace:new(5, 0, 5, 0))
 self:createoptions(optionrow) 
 local editrow = group:new(self.editgroup)
                      :setautosize(true, true)
					            :setalign(alignstyle.TOP)
					            :setmargin(boxspace:new(0, 0, 5, 0))
 self:createeditfield(editrow):initeditevents()   
 self:createsearchbuttons(editrow)
 return self
end

function search:createeditfield(parent)  
  self.edit = edit:new(parent)
                  :setposition(rect:new(point:new(0, 0), dimension:new(200, 20)))
				          :setalign(alignstyle.LEFT)                  
  return self
end

function search:createsearchbuttons(parent)
  self.up = iconbutton:new(parent, psycle.setting():dir("icons") .. "up.png", 0xFFFFFF)
                      :setalign(alignstyle.LEFT)
					            :setmargin(boxspace:new(0, 2, 0, 5))
  self.down = iconbutton:new(parent, psycle.setting():dir("icons") .. "down.png", 0xFFFFFF)
                        :setalign(alignstyle.LEFT)
  local that = self
  function self.up:onclick()
    that.dosearch:emit(that.edit:text(), 
                       search.UP,
                       that.casesensitive:checked(),
                       that.wholeword:checked(),
                       that.useregexp:checked())
  end
  function self.down:onclick()
    that.dosearch:emit(that.edit:text(),
                       search.DOWN,
                       that.casesensitive:checked(),
                       that.wholeword:checked(),
                       that.useregexp:checked())
  end 
  return self
end

function search:initeditevents() 
  local that = self
  function self.edit:onkeydown(ev)         
    if ev:keycode() == keycodes.ENTER then
      local dir = search.DOWN
      if (ev:shiftkey()) then
        dir = search.UP       
      end
      that.dosearch:emit(that.edit:text(), 
                         dir, 
                         that.casesensitive:checked(),
                         that.wholeword:checked(),
                         that.useregexp:checked())
      ev:preventdefault()
    elseif ev:keycode() == keycodes.ESCAPE then
      that:hide():parent():updatealign()
      that.dohide:emit()
    end
  end  
  return self
end

function search:createreplacegroup(parent)
  self.replacegroup = group:new(parent)
                           :setalign(alignstyle.LEFT)
						               :setautosize(true, true)
						               :setmargin(boxspace:new(0, 0, 0, 20))
						               :setpadding(boxspace:new(0, 5, 0, 5))
  self:createreplacefield(self.replacegroup)  
end

function search:createreplacefield(parent)  
  self.replaceactive = checkbox:new(parent)
                               :settext("replace selection with")
							                 :setalign(alignstyle.TOP)
							                 :setmargin(boxspace:new(5, 0, 5, 0))
  self.replacefieldgroup = group:new(parent)
                                :setalign(alignstyle.TOP)
								                :setautosize(true, true)
								                :setmargin(boxspace:new(0, 0, 5, 0))
  self.replacefield = edit:new(self.replacefieldgroup)
                          :disable()
                          :setalign(alignstyle.LEFT)
						              :setautosize(false, false)						  
						              :setcolor(0xA0A0A0)
						              :setbackgroundcolor(0x323232)						  
  local that = self
  function self.replacefield:onkeydown(ev)         
    if ev:keycode() == keycodes.ENTER then      
      that.doreplace:emit()
      ev:preventdefault()
    elseif ev:keycode() == keycodes.ESCAPE then
      that:hide():parent():updatealign()
      that.dohide:emit()
    end
  end
  self:createreplacebutton(self.replacefieldgroup)  
  function self.replaceactive:onclick()
    if self:checked() then
      that.replacefield:enable()
      that.replacebtn:enable()
	    that.replacefield:setbackgroundcolor(0x373533)       
	    that.replacefield:setcolor(0xFFFFFF)
	    that.replacegroup:addornament(ornamentfactory:createlineborder(0x525252))
    else
      that.replacefield:disable()
      that.replacebtn:disable()
      that.replacegroup:removeornaments()	   
	    that.replacefield:setbackgroundcolor(0x323232)	   
	    that.replacefield:setcolor(0xA0A0A0)
	    that.replacefield:fls()
    end
  end
  return self
end

function search:createreplacebutton(parent)
  self.replacebtn = button:new(parent)
                          :settext("replace")
						              :setalign(alignstyle.LEFT)
                          :setmargin(boxspace:new(0, 0, 0, 2))
                          :setposition(rect:new(point:new(0, 0), dimension:new(60, 20)))
                          :disable()
  local that = self
  function self.replacebtn:onclick()
    that.doreplace:emit()
  end  
  return self
end


function search:createoptions(parent)
  self.casesensitive = checkbox:new(parent)
                               :setalign(alignstyle.LEFT)
							                 :settext("match case")
							                 :setmargin(boxspace:new(0, 5, 0, 0))							   
  self.wholeword = checkbox:new(parent)
                           :setalign(alignstyle.LEFT)
						               :settext("match whole words only")
						               :setmargin(boxspace:new(0, 5, 0, 0))						   
  self.useregexp = checkbox:new(parent)
                           :setalign(alignstyle.LEFT)
						               :settext("use regexp")
						               :setmargin(boxspace:new(0, 5, 0, 0))						   
  return self
end

function search:onfocus()  
  self.edit:setfocus()
end

return search
