#pragma once
#include "launcher_settings.h"
#include <vector>
#include <string>
namespace openshao::recomp {
enum class Page {Home,Display,Graphics,Controls,Controller,Keyboard,KeyboardAlternate,Mouse,Audio,Language,Enhancements,Cheats};
struct Row {std::string key,value;bool action=false;};
class Model {
    launcher::Settings initial_,draft_;
    Page page_=Page::Home;
public:
    void Open(const launcher::Settings& settings);
    const launcher::Settings& Draft()const{return draft_;}
    launcher::Settings& Edit(){return draft_;}
    auto CurrentPage()const -> enum Page{return page_;}
    void Go(enum Page page){page_=page;}
    void Back();
    std::vector<Row> Rows()const;
    void Enter(size_t row);
    void Change(size_t row,int direction);
    bool ControllerBinding(size_t row,std::string source);
    bool KeyboardBinding(size_t row,int slot,int key);
    std::string Conflict(size_t row)const;
    void Discard(){draft_=initial_;page_=Page::Home;}
    bool Dirty()const{return draft_!=initial_;}
    std::string Title()const;
};
}
