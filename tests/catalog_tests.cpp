#include "scenes/catalog.h"
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
int main() {
    try {
        const std::set<std::string> expected={"cornell","point","forest","forest-gray","forest-v03",
            "forest-v03-gray","forest-v03-detail","texture","dof","motion","alpha-zero","alpha-half",
            "alpha-one","alpha-double","alpha-pattern","sampling","forest-v05","forest-v05-gray","forest-v05-detail"};
        std::set<std::string> actual;
        std::string joined;
        for(const auto& entry:scene_catalog()) {
            if(!actual.insert(std::string(entry.name)).second) throw std::runtime_error("Duplicate scene name");
            if(entry.description.empty() || !entry.factory) throw std::runtime_error("Incomplete entry");
            if(&find_scene(entry.name)!=&entry) throw std::runtime_error("Lookup not from catalog");
            if(!joined.empty()) joined+='|';
            joined+=entry.name;
            auto world=create_scene(entry.name,"assets");
            if(world.scene.objects.empty()) throw std::runtime_error("Empty factory: "+std::string(entry.name));
        }
        if(actual!=expected) throw std::runtime_error("Legacy names lost or unexpected entry");
        std::ostringstream names;write_scene_names(names);
        if(names.str()!=joined) throw std::runtime_error("Help list differs from dispatch catalog");
        bool rejected=false;
        try {create_scene("unknown-scene","assets");}
        catch(const std::runtime_error& e) {rejected=std::string(e.what()).find("unknown-scene")!=std::string::npos;}
        if(!rejected) throw std::runtime_error("Unknown scene not rejected clearly");
        std::cout<<"PASS 19 unique legacy names, every factory, shared help/dispatch and unknown error\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
