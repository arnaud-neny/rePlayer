///\file
///\brief implementation file for psycle::host::ParamMap.
#pragma once
#include <psycle/host/detail/project.private.hpp>
#include "ParamMap.hpp"
#include "PsycleGlobal.hpp"
#include "PsycleConfig.hpp"
#include "Registry.hpp"
#include "Machine.hpp"
#include "PresetIO.hpp"
#include "MfcUi.hpp"

namespace psycle {
namespace host {
  
ParamMapSkin::ParamMapSkin() {
  PsycleConfig::MachineParam& machine_params = PsycleGlobal::conf().macParam();
  using namespace ui;
  background_color = ToARGB(machine_params.topColor);
  list_view_background_color = ToARGB(machine_params.bottomColor);
  title_background_color = ToARGB(machine_params.titleColor);
  font_color = ToARGB(machine_params.fontTopColor);
  title_font_color = ToARGB(machine_params.fonttitleColor);
  range_end_color = ToARGB(machine_params.fontBottomColor);
  background.reset(OrnamentFactory::Instance().CreateFill(background_color));
  title_background.reset(OrnamentFactory::Instance().CreateFill(title_background_color));  
//   font = ui::mfc::TypeConverter::font(machine_params.font);
//   title_font = ui::mfc::TypeConverter::font(machine_params.font_bold);
}

ParamMap::ParamMap(Machine* machine,ParamMap** windowVar) 
    : machine_(machine),
      windowVar_(windowVar),      
      list_view_(new ui::ListView()),
      cbx_box_(new ui::ComboBox()),
      allow_auto_learn_chk_box_(new ui::CheckBox()),
      machine_param_end_txt_(new ui::Text()),
      root_node_(new ui::Node()) {      
  set_title("Parameter map");
  PreventResize();
  using namespace ui;
  Viewport::Ptr view_(new Viewport());
  view_->SetSave(false);  
  LineBorder* border = new LineBorder(skin_.font_color);
  border->set_border_radius(BorderRadius(5, 5, 5, 5));
  border_.reset(border);
  view_->add_ornament(skin_.background);
  set_viewport(view_);   
  view_->set_aligner(Aligner::Ptr(new DefaultAligner()));      
  FillListView();
  AddWindows();  
  FillComboBox();
  UpdateMachineParamEndText();
}

void ParamMap::UpdateNew(int par,int value) {  
  if (allow_auto_learn_chk_box_->checked()) {
    cbx_box_->set_item_index(par);
    if (HasSelectionChanged()) {    
      UpdateMachineParamEndText();
      ReplaceSelection();
    }
  }
}

void ParamMap::AddWindows() {
  AddListView();
  ui::Window::Ptr client_group = AddClientGroup();
  AddSelectOffsetWithButtonGroup(client_group);
  AddHelpGroup(client_group);  
}

void ParamMap::AddListView() {  
  viewport()->Add(list_view_);
  list_view_->set_background_color(skin_.list_view_background_color);
  list_view_->set_color(skin_.font_color);
  list_view_->set_align(ui::AlignStyle::LEFT);
  list_view_->ViewReport();
  list_view_->EnableRowSelect();
  list_view_->AddColumn("Virtual", 50);
  list_view_->AddColumn("Machine Parameter", 200);  
  list_view_->set_root_node(root_node_);
  list_view_->UpdateList();
  list_view_->change.connect(
    boost::bind(&ParamMap::OnListViewChange, this, _1, _2));
}

ui::Window::Ptr ParamMap::AddClientGroup() {
  ui::Window::Ptr client_group(new ui::Group());  
  viewport()->Add(client_group);
  client_group->set_align(ui::AlignStyle::CLIENT);    
  client_group->set_aligner(ui::Aligner::Ptr(new ui::DefaultAligner()));
  client_group->add_ornament(skin_.background);
  return client_group;
}

void ParamMap::AddSelectOffsetWithButtonGroup(const ui::Window::Ptr& parent) {
  ui::Group::Ptr offset_button_group = CreateTitleGroup(parent, "Offset To Machine Parameter");     
  AddSelectOffsetGroup(offset_button_group);
  AddButtonGroup(offset_button_group);      
}

void ParamMap::AddSelectOffsetGroup(const ui::Window::Ptr& parent) {
  using namespace ui;
  Group::Ptr cbx_group = CreateRow(parent);  
  cbx_group->set_margin(BoxSpace(0, 0, 5, 5));  
  Text::Ptr txt1(new Text("FROM"));
  txt1->set_auto_size(false, false);
  cbx_group->Add(txt1);
  txt1->set_align(AlignStyle::LEFT);
  txt1->set_vertical_alignment(AlignStyle::CENTER);
  txt1->set_color(skin_.font_color);      
  txt1->set_margin(BoxSpace(0, 10, 0, 0));
  txt1->set_justify(JustifyStyle::RIGHTJUSTIFY);  
  txt1->set_position(Rect(Point(), Dimension(50, 0)));
  txt1->set_font(skin_.font);
  cbx_group->Add(cbx_box_);
  cbx_box_->set_auto_size(false, false);
  cbx_box_->set_align(AlignStyle::LEFT);
  cbx_box_->set_position(Rect(Point(0, 0), Dimension(150, 20)));  
  cbx_box_->select.connect(
    boost::bind(&ParamMap::OnComboBoxSelect, this, _1));
  Group::Ptr end_group = CreateRow(parent);
  end_group->set_margin(BoxSpace(5, 0, 10, 5));  
  Text::Ptr txt2(new Text("TO"));  
  end_group->Add(txt2);
  txt2->set_margin(BoxSpace(0, 10, 0, 0));
  txt2->set_justify(JustifyStyle::RIGHTJUSTIFY);
  txt2->set_auto_size(false, true);
  txt2->set_color(skin_.font_color);  
  txt2->set_align(AlignStyle::LEFT);
  txt2->set_position(Rect(Point(), Dimension(50, 0)));
  txt2->set_font(skin_.font);
  end_group->Add(machine_param_end_txt_);    
  machine_param_end_txt_->set_align(AlignStyle::LEFT); 
  machine_param_end_txt_->set_color(skin_.range_end_color);
  machine_param_end_txt_->set_font(skin_.font);
}


void ParamMap::AddButtonGroup(const ui::Window::Ptr& parent) {
  using namespace ui;
  Group::Ptr btn_group = CreateRow(parent);
  btn_group->set_margin(BoxSpace(0, 0, 5, 5));      
  Button::Ptr replace_btn(new Button());
  btn_group->Add(replace_btn);
  replace_btn->set_align(AlignStyle::LEFT);
  replace_btn->set_position(Rect(Point(0, 0), Dimension(100, 20)));
  replace_btn->set_text("replace");    
  replace_btn->click.connect(
    boost::bind(&ParamMap::OnReplaceButtonClick, this, _1));

  btn_group->Add(allow_auto_learn_chk_box_);
  allow_auto_learn_chk_box_->set_align(AlignStyle::LEFT);
  allow_auto_learn_chk_box_->Check();
  allow_auto_learn_chk_box_->set_margin(BoxSpace(0, 0, 0, 5));
  allow_auto_learn_chk_box_->set_position(Rect(Point(), Dimension(100, 20)));
  allow_auto_learn_chk_box_->set_text("Auto Learn");

  Group::Ptr preset_group = CreateRow(parent);  
  preset_group->set_margin(BoxSpace(0, 0, 5, 5));  
  Button::Ptr save_btn(new Button());
  preset_group->Add(save_btn);
  save_btn->set_align(AlignStyle::LEFT);
  save_btn->set_position(Rect(Point(), Dimension(100, 20)));
  save_btn->set_text("Save as Default");
  save_btn->click.connect(
    boost::bind(&ParamMap::OnSaveDefaultButtonClick, this, _1));

  Group::Ptr preset2_group = CreateRow(parent);  
  preset2_group->set_margin(BoxSpace(0, 0, 5, 5));  
  Button::Ptr load_btn(new Button());
  preset2_group->Add(load_btn);
  load_btn->set_align(AlignStyle::LEFT);
  load_btn->set_margin(BoxSpace(0, 5, 0, 0));
  load_btn->set_position(Rect(Point(), Dimension(100, 20)));
  load_btn->set_text("Load Default");
  load_btn->click.connect(
  boost::bind(&ParamMap::OnLoadDefaultButtonClick, this, _1));

  Button::Ptr reset_btn(new Button());
  preset2_group->Add(reset_btn);  
  reset_btn->set_align(AlignStyle::LEFT);
  reset_btn->set_position(Rect(Point(), Dimension(100, 20)));
  reset_btn->set_text("Reset mapping");
  reset_btn->click.connect(
  boost::bind(&ParamMap::OnResetMapButtonClick, this, _1));
}

void ParamMap::AddHelpGroup(const ui::Window::Ptr& parent) {  
  ui::Group::Ptr help_group = CreateTitleGroup(parent, "Help");  
  AddTextLine(help_group, "Shift + Up/Down: Select a range.");
  AddTextLine(help_group, "Auto Learn Checked: Select a range and tweak a knob.");      
}

void ParamMap::AddTextLine(const ui::Window::Ptr& parent, const std::string& text) {
  using namespace ui;
  Text::Ptr line(new Text(text));
  line->set_align(AlignStyle::TOP);
  line->set_font(skin_.font);
  line->set_color(skin_.font_color);
  line->set_margin(ui::BoxSpace(5));
  parent->Add(line);
}

ui::Group::Ptr ParamMap::CreateRow(const ui::Window::Ptr& parent) {
  using namespace ui;
  Group::Ptr header_group(new Group());
  header_group->set_aligner(Aligner::Ptr(new DefaultAligner()));
  header_group->set_auto_size(false, true);
  header_group->set_align(AlignStyle::TOP);
  header_group->set_margin(BoxSpace(5));
  parent->Add(header_group);  
  return header_group;
}

ui::Group::Ptr ParamMap::CreateTitleRow(const ui::Window::Ptr& parent, const std::string& header_text) {
  using namespace ui;
  Group::Ptr header_group = CreateRow(parent);
  header_group->add_ornament(skin_.title_background);  
  Text::Ptr txt(new Text(header_text));
  txt->set_font(skin_.title_font);
  header_group->Add(txt);
  txt->set_color(skin_.title_font_color);  
  txt->set_auto_size(true, true);
  txt->set_margin(BoxSpace(5));
  txt->set_align(AlignStyle::LEFT);
  return header_group;
}

ui::Group::Ptr ParamMap::CreateTitleGroup(const ui::Window::Ptr& parent, const std::string& title) {
  using namespace ui; 
  Group::Ptr title_group(new Group());
  title_group->set_aligner(Aligner::Ptr(new DefaultAligner()));
  title_group->set_auto_size(false, true);
  title_group->set_align(AlignStyle::TOP);
  title_group->set_margin(BoxSpace(5));  
  title_group->add_ornament(border_);
  CreateTitleRow(title_group, title);
  parent->Add(title_group);  
  return title_group;
}

void ParamMap::FillListView() {
  using namespace ui;
  for (int i = 0; i < 256; ++i) {    
    std::stringstream str;
	  str << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << i << " ->";
    Node::Ptr col1_node(new Node(str.str()));                
    std::stringstream str1;    
	  str1 << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << machine_->translate_param(i) 
         << " ["
         << param_name(machine_->translate_param(i)) 
         << "]";
    Node::Ptr col2_node(new Node(str1.str()));
	  col1_node->AddNode(col2_node);
    root_node_->AddNode(col1_node);
  }
}

void ParamMap::FillComboBox() {
  for (int i = 0; i != machine_->GetNumParams(); ++i) {    
    std::stringstream str;
    str << i << " [" << param_name(i) << "]";
    cbx_box_->add_item(str.str());
  }
  cbx_box_->set_item_index(0);
}

void ParamMap::RefreshParamMap() {  
  list_view_->PreventDraw();
  ParamTranslator param;   
  std::vector<ui::Node::Ptr>::iterator it = root_node_->begin();
  for (int i = 0; it != root_node_->end(); ++i, ++it) {
    std::stringstream str;
    str << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << i + cbx_box_->item_index();
    str << " [" << param_name(param.translate(i)) << "]";
    ui::Node::Ptr col2_node = *(*it)->begin();
    col2_node->set_text(str.str());              
  }
  list_view_->EnableDraw();  
  UpdateMachineParamEndText();
}

void ParamMap::UpdateMachineParamEndText() {
  int end = cbx_box_->item_index() + list_view_->selected_nodes().size() - 1;
  if (end >= 0 && end < machine_->GetNumParams()) {         
    machine_param_end_txt_->set_text(param_name(end));
  }
}

void ParamMap::ReplaceSelection() {  
  list_view_->PreventDraw();
  using namespace ui;
  std::vector<Node::Ptr> nodes = list_view_->selected_nodes();
  std::vector<Node::Ptr>::iterator it = nodes.begin();
  for (int i = 0; it != nodes.end(); ++i, ++it) {
    if (i + cbx_box_->item_index() < machine_->GetNumParams()) {
      int virtual_index = (*it)->imp(*list_view_->imp())->position();
      std::stringstream str;
      str << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << i + cbx_box_->item_index();
      str << " [" << param_name(i + cbx_box_->item_index()) << "]";
      Node::Ptr col2_node = *it;
      col2_node->set_text(str.str());
      machine_->set_virtual_param_index(
        virtual_index, i + cbx_box_->item_index());
    } else {
      break;
    }    
  }  
  list_view_->EnableDraw();  
}

bool ParamMap::HasSelectionChanged() {
  bool result = false;
  using namespace ui;
  std::vector<Node::Ptr> nodes = list_view_->selected_nodes();
  std::vector<Node::Ptr>::iterator it = nodes.begin();
  for (int i = 0; it != nodes.end(); ++i, ++it) {
    int virtual_index = (*it)->imp(*list_view_->imp())->position();
    if (i + cbx_box_->item_index() != machine_->translate_param(virtual_index)) {
      result = true;
      break;
    }
  }  
  return result;
}

std::string ParamMap::param_name(int index) const {
  if (index < machine_->GetNumParams()) {
    char s[1024];
    machine_->GetParamName(index, s);
    return s;
  } 
  return "";  
}

void ParamMap::OnSaveDefaultButtonClick(ui::Button&) {
	ParamTranslator param(machine_->param_translator());
	char name[128];
	strcpy(name,machine_->GetDllName());
	if (strcmp(name,"") == 0) { strcpy(name,machine_->GetName()); }
	PresetIO::SaveDefaultMap(PsycleGlobal::conf().GetAbsolutePresetsDir() + "/" + name + ".map", param, machine_->GetNumParams());
}

void ParamMap::OnLoadDefaultButtonClick(ui::Button&) {
	ParamTranslator param;
	char name[128];
	strcpy(name,machine_->GetDllName());
	if (strcmp(name,"") == 0) { strcpy(name,machine_->GetName()); }
	PresetIO::LoadDefaultMap(PsycleGlobal::conf().GetAbsolutePresetsDir() + "/" + name + ".map",param);
	machine_->set_virtual_param_map(param);
	RefreshParamMap();
}

void ParamMap::OnResetMapButtonClick(ui::Button&) {
  for (int i = 0; i < 256; ++i) {
	  machine_->set_virtual_param_index(i, i);
  }
  RefreshParamMap();
}

}   // namespace
}   // namespace
