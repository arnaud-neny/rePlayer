// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2010 members of the psycle project http://psycle.sourceforge.net

// #include "stdafx.h"

#include "Canvas.hpp"
#include "CanvasItems.hpp"
#include <algorithm> // for std::transform
#include <cctype> // for std::tolower

namespace psycle {
namespace host  {
namespace ui {

void DefaultAligner::CalcDimensions() {	
  Dimension current_dim;  
  Rect current_pos;    

  for (iterator i = begin(); i != end(); ++i) {
    Window::Ptr item = *i;
    if (!item->visible()) {
      continue;
    }	    
		calc_window_dim(item);	
		Dimension item_dim = item_dim_;
    // width		
    {
      double diff = current_pos.width();
      switch (item->align()) {
        case AlignStyle::LEFT:
          current_pos.set_left(current_pos.left() + item_dim.width() + item->non_content_dimension().width());
          if (diff == 0) {          
            current_dim.set_width(current_dim.width() + item_dim.width() + item->non_content_dimension().width());
            current_pos.set_right(current_pos.right() + item_dim.width() + item->non_content_dimension().width());
          } else {
            double expand = item_dim.width() + item->non_content_dimension().width() - diff;
            if (expand > 0) {
              current_dim.set_width(current_dim.width() + expand);
              current_pos.set_right(current_pos.right() + expand);
            }
          }          
        break;
        case AlignStyle::RIGHT:        
          if (diff == 0) {          
            current_dim.set_width(current_dim.width() + item_dim.width() + item->margin().width());
            current_pos.set_right(current_pos.right() + item_dim.width() + item->margin().width());
          } else {
            double expand = item_dim.width() + item->margin().width() - diff;
						if (expand > 0) {
							current_dim.set_width(current_dim.width() + expand);
							current_pos.set_right(current_pos.right() + expand);
						}
						else
						if (expand == 0) {
							current_pos.set_right(current_pos.right() - item_dim.width());
						}
					}
					break;
        case AlignStyle::TOP:
				case AlignStyle::BOTTOM:
					if (diff == 0) {
						current_dim.set_width(current_dim.width() + item_dim.width() - item->margin().width());
						current_pos.set_right(current_pos.right() + item_dim.width());
					}
					else {
						double expand = item_dim.width() - diff;
						if (expand > 0) {
							current_dim.set_width(current_dim.width() + expand);
							current_pos.set_right(current_pos.right() + expand);
						}
					}
					current_dim.set_width(current_dim.width());
					break;
				case AlignStyle::CLIENT:
					current_dim.set_width(current_dim.width());
					break;
				default:
					current_dim.set_width((std::max)(item_dim.width(), current_dim.width()));
					break;
			}
		}

		// height		
		{
			double old_height = current_pos.height();
			switch (item->align()) {
			case AlignStyle::TOP:
				current_pos.set_top(current_pos.top() + item_dim.height() + item->non_content_dimension().height());
				if (old_height == 0) {
					current_dim.set_height(current_dim.height() + item_dim.height() + item->non_content_dimension().height());
					current_pos.set_bottom(current_pos.bottom() + item_dim.height() + item->non_content_dimension().height());
				}
				else {
					double expand = item_dim.height() + item->non_content_dimension().height() - old_height;
					current_dim.set_height(current_dim.height() + expand);
					current_pos.set_bottom(current_pos.bottom() + expand);
				}
				// current_dim.set_height(current_dim.height() + item->margin().height());
				break;
			case AlignStyle::LEFT:
			case AlignStyle::RIGHT:
				if (old_height == 0) {
					current_dim.set_height(current_dim.height() + item_dim.height() + item->non_content_dimension().height());
					current_pos.set_bottom(current_pos.bottom() + item_dim.height() + item->non_content_dimension().height());
				}
				else {
					double expand = item_dim.height() + item->non_content_dimension().height() - old_height;
					if (expand > 0) {
						current_dim.set_height(current_dim.height() + expand);
						current_pos.set_bottom(current_pos.bottom() + expand);
					}
				}
				break;
			case AlignStyle::BOTTOM:
				if (old_height == 0) {
					current_dim.set_height(current_dim.height() + item_dim.height());
					current_pos.set_bottom(current_pos.bottom() + item_dim.height());
				}
				else {
					double expand = item_dim.height() - old_height;
					if (expand > 0) {
						current_dim.set_height(current_dim.height() + expand);
						current_pos.set_bottom(current_pos.bottom() + expand);
					}
				}
				current_dim.set_height(current_dim.height() + item->margin().height());
				break;
			case AlignStyle::CLIENT:
				if (item->auto_size_height()) {
					current_dim.set_height(item_dim.height() + item->non_content_dimension().height());
				}
				else {
					current_dim.set_height(current_dim.height() + item->non_content_dimension().height());
				}
				break;
			default:
				current_dim.set_height((std::max)(item_dim.height(), current_dim.height()) + item->non_content_dimension().height());
				break;
			}
		}
	} // end loop   
	
	set_dimension(current_dim);
}

void DefaultAligner::SetPositions() {
	if (group() && group()->size() != 0) {
		prepare_pos_set();
		Window::Ptr client;
		for (iterator i = begin(); i != end(); ++i) {
			(*i)->reset_align_dimension_changed();			
			if (!skip_item(*i)) {				
				if ((*i)->align() == AlignStyle::CLIENT) {
					client = *i;
				} else {				
					update_item_pos_except_client(*i);
				}      
			}
    }
    if (client) {
			update_client_position(client);
    }
	}
}

void DefaultAligner::prepare_pos_set() {
	current_pos_.set(Point::zero(), group()->position().dimension());
}

bool DefaultAligner::skip_item(const Window::Ptr& item) const {
	return (!item->visible() || (item->align() == AlignStyle::ALNONE));
}

void DefaultAligner::update_item_pos_except_client(const Window::Ptr& window) {
	calc_non_content_dimension(window);	
	calc_window_dim(window);
	window->PreventAutoDimension();
	switch (window->align()) {        
		case AlignStyle::LEFT: 				  				  					
			update_left(window);
		break;      
		case AlignStyle::TOP:        
			update_top(window);
		break;
		case AlignStyle::RIGHT:			       
			update_right(window);
		break;
		case AlignStyle::BOTTOM:        					
			update_bottom(window);
		break;
		default:				 				
		break;		
	} // end switch	
	window->RestoreAutoDimension();
}

void DefaultAligner::update_left(const Window::Ptr& window) {	
  Rect new_pos = calc_new_pos_left(window);
  adjust_current_pos_left();
  if (!window->check_position(new_pos)) {
    window->set_position(new_pos);			
  }
}

void DefaultAligner::update_top(const Window::Ptr& window) {
  Rect new_pos = calc_new_pos_top(window);
  adjust_current_pos_top(window);           
	if (!window->check_position(new_pos)) {	
	  window->set_position(new_pos);		
	}
}

void DefaultAligner::update_right(const Window::Ptr& window) {
  Rect new_pos = calc_new_pos_right(window);					
  adjust_current_pos_right();        
	if (!window->check_position(new_pos)) {
	   window->set_position(new_pos);		 		 
	 }
}

void DefaultAligner::update_bottom(const Window::Ptr& window) {
  Rect new_pos = calc_new_pos_bottom(window);					
  adjust_current_pos_bottom();
	if (!window->check_position(new_pos)) {
	  window->set_position(new_pos);		 
	}
}

void DefaultAligner::update_client_position(const Window::Ptr& client) { 
  calc_non_content_dimension(client);
  adjust_current_pos_client(client);   
	if (!client->check_position(current_pos_)) {
    client->PreventAutoDimension();
		client->set_position(current_pos_);		
    client->RestoreAutoDimension();
	}
}

void DefaultAligner::calc_non_content_dimension(const Window::Ptr& window) {	
	non_content_dimension_ = window->non_content_dimension();			
}

void DefaultAligner::calc_window_dim(const Window::Ptr& window) {
 if (window->aligner()) {
    item_dim_ = window->aligner()->dim();
    if (!window->auto_size_width()) {
      item_dim_.set_width(window->dim().width());        
    }
    if (!window->auto_size_height()) {
      item_dim_.set_height(window->dim().height());        
    }
 } else {
	item_dim_ = window->dim();
    if (window->auto_size_width() || window->auto_size_height()) {
      Dimension auto_dim = window->OnCalcAutoDimension();
      if (window->auto_size_width()) {
        item_dim_.set_width(auto_dim.width());
      }
      if (window->auto_size_height()) {
        item_dim_.set_height(auto_dim.height());
      }
    }
  }  
}

Rect DefaultAligner::calc_new_pos_left(const Window::Ptr& window) const {
	Rect result = current_pos_;  
	result.set_width(item_dim_.width());
	if (window->auto_size_height()) {
		result.set_height(item_dim_.height());
	} else {
		result.decrease(BoxSpace(0, 0, non_content_dimension_.height(), 0));
	}
	return result;
}

Rect DefaultAligner::calc_new_pos_top(const Window::Ptr& window) const {
	Rect result = current_pos_;  
	result.set_height(item_dim_.height());
	if (window->auto_size_width()) {
		result.set_width(item_dim_.width());
	} else {
		result.decrease(BoxSpace(0, non_content_dimension_.width(), 0, 0));
	}
	return result;
}

Rect DefaultAligner::calc_new_pos_right(const Window::Ptr& window) const {
	Rect result = current_pos_;	    
	result.set_left(current_pos_.right() - item_dim_.width() - non_content_dimension_.width());
	result.set_right(current_pos_.right() - non_content_dimension_.width());
	if (window->auto_size_height()) {
		result.set_height(item_dim_.height());
	} else {
		result.decrease(BoxSpace(0, 0, non_content_dimension_.height(), 0));
	}	
	return result;
}

Rect DefaultAligner::calc_new_pos_bottom(const Window::Ptr& window) const {
	Rect result = current_pos_;		
	result.set_top(current_pos_.bottom() - item_dim_.height() - non_content_dimension_.height());
	result.set_bottom(current_pos_.bottom() - non_content_dimension_.height());
  if (window->auto_size_width()) {
		result.set_width(item_dim_.width());
	} else {
    result.decrease(BoxSpace(0, non_content_dimension_.width(), 0, 0));
	}
	return result;
}

void DefaultAligner::adjust_current_pos_left() {		
	current_pos_.set_left(current_pos_.left() + item_dim_.width() + non_content_dimension_.width());
}

void DefaultAligner::adjust_current_pos_top(const Window::Ptr window) {
	current_pos_.set_top(current_pos_.top() + item_dim_.height() + non_content_dimension_.height());  
}

void DefaultAligner::adjust_current_pos_right() {
	current_pos_.set_right(current_pos_.right() - item_dim_.width() - non_content_dimension_.width());
}

void DefaultAligner::adjust_current_pos_bottom() {		
  current_pos_.set_bottom(current_pos_.bottom() - item_dim_.height() - non_content_dimension_.height());
}

void DefaultAligner::adjust_current_pos_client(const Window::Ptr& window) {	
	current_pos_.decrease(
		BoxSpace(
			0, 
			non_content_dimension_.width(),
			non_content_dimension_.height(),
			0
		));
}

// WrapAligner

void WrapAligner::CalcDimensions() {}

void WrapAligner::SetPositions() {  
  if (group()->area().bounds().empty()) {
    return;
  }
  Window::Ptr client;
  Rect current_position(Point(), group()->aligner()->dim());
  for (iterator it = begin(); it != end(); ++it) {
    Window::Ptr window = *it;
    if (!window->visible()) {
      continue;
    }    
    calc_window_dim(window);
    window->set_position(Rect(current_position.top_left(), item_dim_));
    if (current_position.left() > current_position.right()) {
      current_position.set_left(0);
      current_position.offset(0, 100);     
    }
  }
}

void WrapAligner::calc_window_dim(const Window::Ptr& window) {
	item_dim_ = window->aligner() ? window->aligner()->dim() : window->dim();
  if (window->aligner()) {
    if (!window->auto_size_width()) {
      item_dim_.set_width(window->dim().width());
    }
    if (!window->auto_size_height()) {
      item_dim_.set_height(window->dim().height());        
    }
	}
}

// GridAligner
void GridAligner::CalcDimensions() {  
  Dimension itemmax;
  for (iterator i = begin(); i != end(); ++i) {
    Window::Ptr item = *i;
    if (!item->visible()) {
      continue;
    }        
    Dimension item_dim = item->aligner() ? item->aligner()->dim() : item->dim();   
    if (item->aligner()) {
      if (!item->auto_size_width()) {
        item_dim.set_width(item->dim().width());
      }
      if (!item->auto_size_height()) {
        item_dim.set_height(item->dim().height());
      }
    }
    if (item->align() != AlignStyle::ALNONE) {      
      item_dim.set(item_dim.width(), item_dim.height());
    }       
    itemmax.set_width((std::max)(itemmax.width(), item_dim.width()));
    itemmax.set_height((std::max)(itemmax.height(), item_dim.height()));    
  } // end loop   
  
  Dimension current_dim(itemmax.width()*col_num_, itemmax.height()*row_num_);  
  set_dimension(current_dim);
}

void GridAligner::SetPositions() {
  if (!group() || group()->area().bounds().empty()) {
    return;
  }
  Window::Ptr client;
  Rect current_position(Point(), group()->aligner()->dim());
  double cell_width = current_position.width() / col_num_;
  double cell_height = current_position.height() / row_num_;
  int pos(0);
  for (iterator i = begin(); i != end(); ++i) {
    Window::Ptr item = *i;
    if (!item->visible()) {
      continue;
    }    
    item->set_position(Rect(current_position.top_left(), 
                       Dimension(cell_width, cell_height)));    
    ++pos;
    if (pos < col_num_) {            
      current_position.offset(cell_width, 0);      
    } else {      
      current_position.offset(0, cell_height);
      current_position.set_left(0);
      pos = 0;
    }  
  } // end loop     
}

void GridAligner::Realign() {}


// Canvas
void Viewport::Init() {
  set_auto_size(false, false);
  set_position(Rect(Point::zero(), Dimension(500, 500)));
  steal_focus_ = fls_prevented_ = draw_background_prevented_ = false;
  save_ = true; 
}

void Viewport::OnSize(const Dimension& dimension) {
  Group::OnSize(dimension);
  try {    
    UpdateAlign();    
  } catch (std::exception& e) {
    alert(e.what());
  }
}

void Viewport::StealFocus(const Window::Ptr& item) {  
  steal_focus_ = true;
}

void Viewport::set_properties(const Properties& properties) {
  Properties::Container::const_iterator it = properties.elements().begin();
  for ( ; it != properties.elements().end(); ++it) {
    if (it->first == "backgroundcolor") {
      background_.reset(
          OrnamentFactory::Instance().CreateFill(it->second.argb_value()));
      RemoveOrnaments();
      add_ornament(background_);
    };   
  }  
}

void Viewport::Invalidate(const Region& rgn) {
  if (!fls_prevented_) {
    if (IsSaving()) {
      save_rgn_->Union(rgn);
    } else { 
      Window::Invalidate(rgn);      
    }
  }  
}

void Viewport::Flush() {    
  if (!fls_prevented_) {  
    Window::Invalidate(*save_rgn_.get());
  }  
  save_rgn_->Clear();    
}

void Viewport::Invalidate() {  
  if (!fls_prevented_) {
    Window::Invalidate();    
  }  
}  

void Viewport::InvalidateSave() {
  if (!fls_prevented_) {     
    Invalidate(*save_rgn_.get());
  }
}

} // namespace ui
} // namespace host
} // namespace psycle
