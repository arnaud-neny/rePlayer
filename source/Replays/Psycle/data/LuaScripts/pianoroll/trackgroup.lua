local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local boxspace = require("psycle.ui.boxspace")
local alignstyle = require("psycle.ui.alignstyle")
local keycodes = require("psycle.ui.keycodes")
local group = require("psycle.ui.group")
local toolicon = require("psycle.ui.toolicon")
local trackview = require("trackview")
local trackheaderview = require("trackheaderview")
local miniview = require("miniview")
local switch = require("switch")
local toolbar = require("psycle.ui.toolbar")
local cfg = require("psycle.config"):new("PatternVisual")

local trackviewgroup = group:new()

local icondir = cfg:luapath() .. "\\psycle\\ui\\icons\\"

function trackviewgroup:new(parent, ...)
  local m = group:new(parent)
  setmetatable(m, self)
  self.__index = self    
  m:init(...)
  return m
end

function trackviewgroup:init(main, keymap, sequence, cursor, scroller)
  self:setalign(alignstyle.TOP)
      :setautosize(false, true)
      :setmargin(boxspace:new(5, 0, 0, 0))
  local that = self  
  self:inittracktoolbar(self)  
  local trackslefticon = toolicon:new(self)
                                 :settext("<")
                                 :setalign(alignstyle.LEFT)
                                 :setautosize(false, false)
                                 :setposition(rect:new(point:new(), dimension:new(20, 17)))
  function trackslefticon:onclick()     
    cursor:scrollleft()  
  end
    
  self.endlessmode = switch:new(self)
                           :setalign(alignstyle.RIGHT)
                           :setmargin(boxspace:new(0, 10, 5, 10))
  function self.endlessmode:onclick()
    main:toggleseqdisplay()
  end  
  self.miniview = miniview:new(self, keymap, sequence)
                          :setalign(alignstyle.RIGHT)
                          :setautosize(false, false)
                          :setposition(rect:new(point:new(), dimension:new(100, 17)))  
                          :setmargin(boxspace:new(0, 0, 5, 0)) 
 -- scroller:addlistener(self.miniview)  
  scroller:addhorizontalinput(self.miniview.hscroll)
  scroller:addverticalinput(self.miniview.vscroll)
  local tracksrighticon = toolicon:new(self)
                                  :settext(">")
                                  :setalign(alignstyle.RIGHT)
                                  :setautosize(false, false)
                                  :setposition(rect:new(point:new(), dimension:new(20, 17)))
  function tracksrighticon:onclick()
    cursor:scrollright(that.trackheaderview:visibletracks())
  end
  self.trackheaderview = trackheaderview:new(self, cursor)
                                        :setautosize(false, true)
							                          :setalign(alignstyle.TOP)                                        
  self.trackview = trackview:new(self, sequence, cursor, scroller)
                            :setautosize(false, true)
							              :setalign(alignstyle.TOP)
                            :setmargin(boxspace:new(0, 0, 2, 0))
  cursor.listeners_:addlistener(self.trackview)                            
  cursor.listeners_:addlistener(self.trackheaderview)
end

function trackviewgroup:inittracktoolbar(parent)  
  local t = toolbar:new(parent)
                   :setautosize(fale, false)
                   :setalign(alignstyle.LEFT)
                   :setposition(rect:new(point:new(), dimension:new(25, 20)))
  self.trackicons = {  
    itrackfilter = toolicon:new(t, icondir .. "filter.png", 0xFFFFFF),
    itrackeditmode = toolicon:new(t, icondir .. "trackeditmode.png", 0xFFFFFF):enabletoggle()  
  }
  for _, icon in pairs(self.trackicons) do    
    self:formattoolicon(icon)
    icon:setalign(alignstyle.TOP)
    icon:setposition(rect:new(point:new(), dimension:new(20, 20)))
  end 
  self.trackicons.itrackfilter:enabletoggle() 
end

function trackviewgroup:formattoolicon(icon)    
  icon:setautosize(false, false)
      :setverticalalignment(toolicon.CENTER)
      :setjustify(toolicon.CENTERJUSTIFY)
      :setposition(rect:new(point:new(), dimension:new(20, 0)))
  return self
end

function trackviewgroup:addstatuslistener(listener)
  self.trackview:addstatuslistener(listener)
  self.trackheaderview:addstatuslistener(listener)
  return self
end

return trackviewgroup