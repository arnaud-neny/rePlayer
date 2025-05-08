-- psycle tabgroup (c) 2015 by psycledelics
-- File: tabgroup.lua
-- copyright 2015 members of the psycle project http://psycle.sourceforge.net
-- This source is free software ; you can redistribute it and/or modify it under
-- the terms of the GNU General Public License as published by the Free Software
-- Foundation ; either version 2, or (at your option) any later version.  

local point = require("psycle.ui.point")
local dimension = require("psycle.ui.dimension")
local rect = require("psycle.ui.rect")
local boxspace = require("psycle.ui.boxspace")
local viewport = require("psycle.ui.viewport")
local font = require("psycle.ui.font")
local fontinfo = require("psycle.ui.fontinfo")
local group = require("psycle.ui.group")
local text = require("psycle.ui.text")
local ornamentfactory = require("psycle.ui.ornamentfactory"):new()
local toolicon = require("psycle.ui.toolicon")
local frame = require("psycle.ui.frame")
local popupframe = require("psycle.ui.popupframe")
local popupmenu = require("psycle.ui.popupmenu")
local node = require("psycle.node")
local signal = require("psycle.signal")
local framealigner = require("psycle.ui.framealigner")
local alignstyle = require("psycle.ui.alignstyle")

local tabgroup = group:new()
local cfg = require("psycle.config"):new("MacParamVisual")
local serpent = require("psycle.serpent")

tabgroup.picdir = cfg:luapath() .. "\\psycle\\ui\\icons\\"

function tabgroup:new(parent)  
  local c = group:new()  
  setmetatable(c, self)  
  self.__index = self  
  c:init()
  if parent ~= nil then
    parent:add(c)
  end
  return c
end

function weakref(data)
    local weak = setmetatable({content=data}, {__mode="v"})
    return function() return weak.content end
end

function tabgroup:init()  
  self.frames = {}
  self.dopageclose = signal:new()
  self.hasclosebutton_ = true 
  self:setautosize(false, false)
  self.tabbar = group:new(self)
                     :setautosize(false, true)
                     :setalign(alignstyle.TOP)
                     :setpadding(boxspace:new(0, 0, 3, 0))
  self.tabbar.moreicon = toolicon:new(self.tabbar, tabgroup.picdir .. "arrow_more.bmp", 0xFFFFFF)
                                 :setautosize(false, false)                        
                                 :setverticalalignment(toolicon.CENTER)
                                 :setjustify(toolicon.CENTERJUSTIFY)
                                 :setalign(alignstyle.RIGHT)    
                                 :setmargin(boxspace:new(0, 2, 0, 2))
  local that = self
  self:inittabpopupmenu()
  self:initframepopupmenu()
  function self.tabbar.moreicon:onclick()    
    self.f = popupframe:new()    
    self.c = viewport:new()
                   :invalidatedirect()
    local that1 = self    
    function self.c:onmousedown(ev)
      that1.f:hide()
    end
    local g = group:new(self.c)
                   :setautosize(false, true)
                   :setalign(alignstyle.TOP)    
    self.f:setviewport(self.c)
    self.c:addornament(ornamentfactory:createfill(0xFF292929))
    self.c:addornament(ornamentfactory:createlineborder(0xFF696969))
    function fun(item)
      local t = text:new(g)
                    :setcolor(0xFFCACACA)
                    :settext(item.text:text())
                    :setautosize(false, true)
                    :setpadding(boxspace:new(5))
                    :setverticalalignment(alignstyle.CENTER)
                    :setjustify(alignstyle.CENTER)
                    :setalign(alignstyle.TOP)
      function t:onmousedown()        
        that:setactivepage(item.page)
      end
      function t:onmouseenter()                            
        self:setpadding(boxspace:new(4))
        self:addornament(ornamentfactory:createlineborder(0xFF696969))
      end      
      function t:onmouseout()                       
        self:setpadding(boxspace:new(5))
        self:removeornaments()
      end
    end
    that:traverse(fun, that.tabs:windows())
    self.c:updatealign()
    self.f:hidedecoration()
          :setposition(
              rect:new(point:new(self:desktopposition():right() - 200,
                                 self:desktopposition():bottom()),
                                 dimension:new(200, g:position():height() + 6)))
          :show()            
  end  
  self.tabs = group:new(self.tabbar)
                   :setautosize(false, true)
                   :setalign(alignstyle.CLIENT)                   
  self.children = group:new(self)                       
                       :setalign(alignstyle.CLIENT)
  self.lastinsert_ = nil;                      
end

function tabgroup:typename()  
  return "tabgroup"
end

function tabgroup:setlabel(page, text)
  function f(item)  
    if item.page == page then 
     item.text:settext(text)
    end
  end
  self:traverse(f, self.tabs:windows())
  self.tabs:updatealign()
end

function tabgroup:activepage()
  if self.activeframepage_ ~= nil then
    return self.activeframepage_
  else
    return self.activepage_
  end
end

function tabgroup:setactiveheader(page)  
   if page ~= self.activepage_ then    
    local tabs = self.tabs:windows()
    local index, oldindex = 0, 0
    for i=1, #tabs do
      local tab = tabs[i]
      if tab.page == page then
        index = i       
      end      
      if tab.page == self.activepage_ then
        oldindex = i       
      end
    end        
    if self.activepage_ then      
      if oldindex ~= 0 then
        tabs[oldindex]:setskinnormal()
      end
    end    
    if index ~= 0 then    
      tabs[index]:setskinhighlight()
    end
  end    
end

function tabgroup:setactivepage(page, preventalign)
  if page and self.activepage_ ~= page  then    
    self:setactiveheader(page)    
    page:show() 
    if self.activepage_ then
      self.activepage_:hide()
    end    
    self.activepage_ = page;
    if not preventalign then
      self.children:updatealign() 
    end
  end
end

function tabgroup:enableclosebutton()
  self.hasclosebutton_ = true
  return self
end

function tabgroup:disableclosebutton()
  self.hasclosebutton_ = false
  return self
end

function tabgroup:removeall()
  self.activepage_ = nil
  self.activeframepage_ = nil
  self.tabs:removeall()
  collectgarbage()
  self.children:removeall()
  collectgarbage()
  self:updatealign()
end

function tabgroup:removepage(page)    
  local tabs = self.tabs:windows()  
  for i=1, #tabs do
    if tabs[i].page == page then    
      self:removepagebyheader(tabs[i])
      break;
    end
  end      
end

function tabgroup:removepagebyheader(header)
  local isactivepage = (header.page == self:activepage())    
  self:root():invalidateontimer()
  local pages = self.children:windows()
  local idx = self.children:windowindex(header.page)  
  if header.page.onclose then
    header.page:onclose()
  end
  self.children:remove(header.page)
  self.tabs:remove(header) 
  if idx == #pages then
    idx = idx -1
  end
  if (isactivepage) then
    self.activepage_ = nil  
    local pages = self.children:windows()  
    self:setactivepage(pages[idx], true)
  end  
  self:flagnotaligned():updatealign()
  self:root():invalidatedirect():flush()  
end

function tabgroup:addpage(page, label, preventclose)
  self:root():invalidateontimer()
  page:setautosize(false, false)
  page:setalign(alignstyle.CLIENT)
  self:createheader(page, label, preventclose)
  self.children:add(page)
  self:setactivepage(page, true)  
  self:flagnotaligned():updatealign()
  self:root():invalidatedirect():flush()
  self.lastinsert_ = weakref(page);  
end

function tabgroup:lastinsert()
  return self.lastinsert_();
end

function tabgroup:createheader(page, label, preventclose)  
  local header = group:new()
                      :setautosize(true, true)
                      :setalign(alignstyle.LEFT)
                      :setposition(rect:new(point:new(), dimension:new(100, 20)))
                      :viewdoublebuffered()
  header.page = page    
  local font = font:new():setfontinfo(fontinfo:new():setsize(13))  
  header.text = text:new(header)
                    :settext(label)
                    :setfont(font)
                    :setalign(alignstyle.LEFT)
                    :setpadding(boxspace:new(3))
  local that = self
  if preventclose == nil or preventclose == false then
    local font = font:new():setfontinfo(fontinfo:new():setsize(13))
    header.close = text:new(header)                           
                       :setcolor(that.headerclosecolor_)
                       :settext("x")
                       :setjustify(text.CENTERJUSTIFY)                       
                       :setverticalalignment(alignstyle.CENTER)            
                       :setalign(alignstyle.LEFT)
                       :setautosize(false, true)
                       :setposition(rect:new(point:new(), dimension:new(15, 15)))
                       :setmargin(boxspace:new(3, 5, 0, 5))
                       :setfont(font)    
    function header.close:onmousedown()
      local ev = {}
      ev.page = self:parent().page
      that.dopageclose:emit(ev)
      that:removepagebyheader(self:parent())
    end
    function header.close:onmouseenter(ev)
      self:setcolor(that.headerclosehovercolor_)
      self:addornament(ornamentfactory:createcirclefill(that.headerclosehoverbackgroundcolor_))
    end
    function header.close:onmouseout(ev)
      self:setcolor(that.headerclosecolor_)
      self:removeornaments()
    end
  end
  self.tabs:add(header)
  function header:setskinhighlight()        
    self:setpadding(boxspace:new(2))
    self:removeornaments():addornament(ornamentfactory:createfill(that.headeractivebackgroundcolor_)) 
    self:addornament(ornamentfactory:createlineborder(that.headerbordercolor_))    
    self.text:setcolor(that.headeractivecolor_)     
  end
  function header:setskinnormal()
    self:setpadding(boxspace:new(3))
    self:removeornaments():addornament(ornamentfactory:createfill(that.headerbackgroundcolor_)) 
    self.text:setcolor(that.headercolor_)
  end    
  header:setskinnormal()    
  function header:onmouseenter(ev)       
    if self.page ~= that:activepage() then    
      self.text:setcolor(that.headerhovercolor_)        
    end
  end
  function header:onmouseout(ev)
    if self.page ~= that:activepage() then    
      self.text:setcolor(that.headercolor_)   
    end
  end  
  function header:onmousedown(ev)
    that:setactivepage(self.page)    
    if ev:button() == 1 then          
      self.isdragactive = false
    elseif ev:button() == 2 then
      that.tabpopupmenu:track(self:desktopposition():topleft():offset(5, 5))
      that.tabpopupmenu.headertext = self.text:text()
    end    
  end
  function header:onmousemove(ev)   
   if ev:button() == 1 then
      local diff = ev:clientpos():x()- self:absoluteposition():left()
      if not self.isdragactive then
        self.dragbase = diff
        self:bringtotop()
        self:capturemouse()
        self.isdragactive = true
      end        
      self:setposition(self:position():setleft(self:position():left() + diff - self.dragbase))
          :setalign(alignstyle.NONE) 
    end    
  end
  function header:onmouseup(ev)
    if ev:button() == 1 and self.isdragactive then
      self:releasemouse()
      local tabs = that.tabs:windows()
      local swaptab = nil
      for i=1, #tabs do
        local tab = tabs[i]
        if tab ~= self and (tab:position():left() + (tab:position():width() / 2)) > self:position():left() then
          swaptab = tab          
          break           
        end
      end
      local p = self:parent()
      p:remove(self)
     if swaptab then
       p:insert(swaptab, self) 
     else
       p:add(self)
     end   
     self:setalign(alignstyle.LEFT)
     that.tabs:updatealign()
    end
  end
  return header
end


function tabgroup:inittabpopupmenu() 
  self.tabpopuprootnode = node:new()
  local node1 = node:new():settext("Close")
  self.tabpopuprootnode:add(node1)
  local node2 = node:new():settext("Show As Window")
  self.tabpopuprootnode:add(node2)
  self.tabpopupmenu = popupmenu:new():setrootnode(self.tabpopuprootnode):update()    
  local that = self    
  function self.tabpopupmenu:onclick(node)       
    if node:text() == "Show As Window" then      
      that:openinframe(that.activepage_, self.headertext)      
    elseif node:text() == "Close" then
      local ev = {}
      ev.page = that:activepage()
      that.dopageclose:emit(ev)
      that:removepage(ev.page)
    end
  end  
end

function tabgroup:initframepopupmenu() 
  self.framepopuprootnode = node:new()
  local node1 = node:new():settext("Close")
  self.framepopuprootnode:add(node1)
  local node2 = node:new():settext("Dock To Tabs")
  self.framepopuprootnode:add(node2)
  self.framepopupmenu = popupmenu:new():setrootnode(self.framepopuprootnode):update()    
  local that = self
  function self.framepopupmenu:onclick(node)       
    if node:text() == "Dock To Tabs" then      
      that:openintab(self.frame_)      
      self.frame_ = nil
    elseif node:text() == "Close" then
      local page = self.frame_:view():windows()[1]
      local ev = {}
      ev.page = page
      that.dopageclose:emit(ev)
      that:unregisterframe(self.frame_)      
      that:removeframepagepointer()     
    end    
  end  
end

function tabgroup:openinframe(page, name)    
  local viewport = viewport:new()  
  self:removepage(page)
  self.activepage_ = nil  
  viewport:add(page)
  local frame = self:createframe(viewport, name)
  self.framepopupmenu.frame_ = frame
  frame:setpopupmenu(self.framepopupmenu)
  frame:show(framealigner:new():sizetoscreen(0.3, 0.5))
end

function tabgroup:createframe(viewport, name)  
  local frame = frame:new():setviewport(viewport):settitle(name)
  viewport:invalidatedirect()
  frame.tabgroup = self
  self.frames[#self.frames+1] = frame
  local that = self
  function frame:oncontextpopup(ev)
     that.framepopupmenu.frame_ = self
  end
  function frame:onclose(ev)
    local page = self:view():windows()[1]
    local ev = {}
    ev.page = page
    that.dopageclose:emit(ev)
    for i = 1, #that.frames do
      if that.frames[i] == self then
        that.frames[i] = nil
        break
      end
    end
    that.framepopupmenu.frame_ = nil
    that.activeframe_ = nil
    that.activeframepage_ = nil
    self = nil  
  end
  function frame:onfocus()
   local page = self:view():windows()[1]
   self.tabgroup.activepage_ = nil
   self.tabgroup.activeframepage_ = page
   self.tabgroup.framepopupmenu.frame_ = self
  end
  function frame:onkillfocus()
    self.activeframepage_ = nil
  end
  return frame
end

function tabgroup:openintab(frame)     
  local page = frame:view():windows()[1]
  local name = frame:title()
  frame:view():remove(page)
  self:unregisterframe(frame)  
  self:removeframepagepointer()
  self:hideallpages()
  self:setnormalskintoalltabs()
  self:addpage(page, name)  
end

function tabgroup:hideallpages()
  local pages = self.children:windows()
  for i=1, #pages do
    pages[i]:hide()
  end
end

function tabgroup:removeframepagepointer()
  self.activepage_ = nil
  self.activeframepage_ = nil
  self.framepopupmenu.frame_ = nil
end

function tabgroup:setnormalskintoalltabs()
  local tabs = self.tabs:windows()
  for i=1, #tabs do
    tabs[i]:setskinnormal()
  end
end  


function tabgroup:unregisterframe(frame)
  for i = 1, #self.frames do
    if self.frames[i] == frame then
      frame:hide()
      self.frames[i] = nil      
      break
    end  
  end
end

function tabgroup:traverse(f, arr)  
  for i=1, #arr do f(arr[i]) end  
  return self
end

function tabgroup:childat(i)
  return self.children:windows()[i]
end

function tabgroup:transparent()  
  return false
end

function tabgroup:setcolor(color) 
  self.color_ = color  
  return self
end

function tabgroup:setbackgroundcolor(color) 
  self.backgroundcolor_ = color
  self.children:removeornaments():addornament(ornamentfactory:createboundfill(color))
  return self
end

function tabgroup:setproperties(properties) 
  if properties.color then    
    self:setcolor(properties.color:value())
  end  
  if properties.backgroundcolor then    
    self:setbackgroundcolor(properties.backgroundcolor:value())
  end
  if properties.tabbarcolor then
    self.tabbarcolor_ = properties.tabbarcolor:value()
    self.tabs:removeornaments():addornament(ornamentfactory:createfill(self.tabbarcolor_))
  end
  if properties.headercolor then
    self.headercolor_ = properties.headercolor:value()
  end
  if properties.headerbackgroundcolor then
    self.headerbackgroundcolor_ = properties.headerbackgroundcolor:value()
  end  
  if properties.headeractivecolor then
    self.headeractivecolor_ = properties.headeractivecolor:value()
  end
  if properties.headeractivebackgroundcolor then  
    self.headeractivebackgroundcolor_ = properties.headeractivebackgroundcolor:value()
  end
  if properties.headerhovercolor then
    self.headerhovercolor_ = properties.headerhovercolor:value()
  end
  if properties.headerhoverbackgroundcolor then
    self.headerhoverbackgroundcolor_ = properties.headerhoverbackgroundcolor:value()
  end  
  if properties.headerbordercolor then
    self.headerbordercolor_ = properties.headerbordercolor:value()
  end
  if properties.headerclosecolor then
    self.headerclosecolor_ = properties.headerclosecolor:value()
  end
  if properties.headerclosehovercolor then
    self.headerclosehovercolor_ = properties.headerclosehovercolor:value()
  end
  if properties.headerclosehoverbackgroundcolor then
    self.headerclosehoverbackgroundcolor_ = properties.headerclosehoverbackgroundcolor:value()
  end
  if self.tabs then
	  local tabs = self.tabs:windows()  
	  for i=1, #tabs do    
		local header = tabs[i]
		if header.page ~= self:activepage() then
		  header:setskinnormal()
		else
		header:setskinhighlight()
		end
	  end
  end      
  self:fls()  
end

return tabgroup