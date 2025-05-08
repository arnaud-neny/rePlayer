///\file
///\brief interface file for psycle::host::CParamMap
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Psycle.hpp"
#include "CanvasItems.hpp"

namespace psycle {
namespace host {

class Machine;

class ParamMapSkin {
 public:
  ParamMapSkin();
  
  ui::ARGB background_color;  
  ui::ARGB list_view_background_color;
  ui::ARGB title_background_color;
  ui::ARGB font_color;
  ui::ARGB title_font_color;
  ui::ARGB range_end_color;
  ui::Ornament::Ptr background;  
  ui::Ornament::Ptr title_background;
  ui::Font font;
  ui::Font title_font;
};

class ParamMap : public ui::Frame {
 public:
	ParamMap(Machine* machine,ParamMap** windowVar);
  
  virtual void OnClose() { 
	if(windowVar_!= NULL) *windowVar_ = NULL;
    delete this;  
  }
  void UpdateNew(int par, int value);  

 private:
  void AddWindows();  
  void AddListView();
  ui::Window::Ptr AddClientGroup();
  void AddSelectOffsetWithButtonGroup(const ui::Window::Ptr& parent);
  void AddSelectOffsetGroup(const ui::Window::Ptr& parent);
  void AddButtonGroup(const ui::Window::Ptr& parent);
  void AddHelpGroup(const ui::Window::Ptr& parent);
  void AddTextLine(const ui::Window::Ptr& parent, const std::string& text);
  ui::Group::Ptr CreateRow(const ui::Window::Ptr& parent);
  ui::Group::Ptr CreateTitleRow(const ui::Window::Ptr& parent, const std::string& header_text);
  ui::Group::Ptr CreateTitleGroup(const ui::Window::Ptr& parent, const std::string& title);
  void FillListView();  
  void FillComboBox();  
  void RefreshParamMap();  
  void UpdateMachineParamEndText();
  void ReplaceSelection();
  bool HasSelectionChanged();
  std::string param_name(int index) const;

  void OnReplaceButtonClick(ui::Button&) { ReplaceSelection(); }
  void OnSaveDefaultButtonClick(ui::Button&);
  void OnLoadDefaultButtonClick(ui::Button&);
  void OnResetMapButtonClick(ui::Button&);    
  void OnListViewChange(ui::ListView&, const ui::Node::Ptr&) { 
    UpdateMachineParamEndText();
  }
  void OnComboBoxSelect(ui::ComboBox&) {  UpdateMachineParamEndText(); }
         
  ui::Node::Ptr root_node_;
  Machine* machine_;
  ParamMap** windowVar_;  
  ui::ListView::Ptr list_view_;
  ui::ComboBox::Ptr cbx_box_;
  ui::CheckBox::Ptr allow_auto_learn_chk_box_;
  ui::Text::Ptr machine_param_end_txt_;  
  boost::shared_ptr<ui::Ornament> border_;
  boost::shared_ptr<ui::Ornament> text_border_;

  ParamMapSkin skin_;
};

}   // namespace
}   // namespace
