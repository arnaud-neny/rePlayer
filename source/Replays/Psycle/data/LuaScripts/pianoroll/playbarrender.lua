local point = require("psycle.ui.point")

local playbarrender = {}

local colors = {
  barcolor = 0xFF999999,
  beatcolor = 0xFF484848,
  linecolor = 0xFF232323,
  playcolor = 0xFF1144CC
}

function playbarrender.draw(g, view, playposition)   
  g:setcolor(view.colors.PLAYBAR)
  playbarrender.render(g, view, playposition)
end

function playbarrender.clear(g, view, playposition)  
  playbarrender.setclearcolor(g, playposition)
  playbarrender.render(g, view, playposition)            
end

function playbarrender.setclearcolor(g, playposition)
  if playposition % 1 == 0 then
    if playposition % 4 == 0 then 
      g:setcolor(colors.barcolor)
    else
      g:setcolor(colors.beatcolor)
    end
  else
    g:setcolor(colors.linecolor)
  end
end  

function playbarrender.render(g, view, playposition)
  local x = playposition * view:zoom():width()
  g:drawline(point:new(x, - view:dy()), 
             point:new(x, view:position():height() - view:dy()))
end

return playbarrender
