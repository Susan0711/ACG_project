#include "scenes/catalog.h"
#include "forest_v05.h"
#include <ostream>
#include <stdexcept>
const std::vector<SceneEntry>& scene_catalog() {
    static const std::vector<SceneEntry> entries={
        {"cornell","Cornell diffuse/mirror/glass and area light",[](const std::string&){return make_world(false,false);}},
        {"point","Cornell geometry with point light",[](const std::string&){return make_world(true,false);}},
        {"forest","Original forest",[](const std::string& r){return make_forest(false,r,false);}},
        {"forest-gray","Original forest gray",[](const std::string& r){return make_forest(true,r,false);}},
        {"forest-v03","Forest v0.3",[](const std::string& r){return make_forest_v03(false,r,false);}},
        {"forest-v03-gray","Forest v0.3 gray",[](const std::string& r){return make_forest_v03(true,r,false);}},
        {"forest-v03-detail","Forest v0.3 detail",[](const std::string& r){return make_forest_v03(false,r,true);}},
        {"texture","Original wood UV square",make_texture_scene},
        {"dof","Three depth planes; enable lens via CLI",[](const std::string& r){return make_effect_scene("dof",r);}},
        {"motion","Translating sphere; set shutter via CLI",[](const std::string& r){return make_effect_scene("motion",r);}},
        {"alpha-zero","Zero coverage sheet",[](const std::string& r){return make_effect_scene("alpha-zero",r);}},
        {"alpha-half","Half coverage sheet",[](const std::string& r){return make_effect_scene("alpha-half",r);}},
        {"alpha-one","Opaque sheet",[](const std::string& r){return make_effect_scene("alpha-one",r);}},
        {"alpha-double","Two half coverage sheets",[](const std::string& r){return make_effect_scene("alpha-double",r);}},
        {"alpha-pattern","Original leaf coverage",[](const std::string& r){return make_effect_scene("alpha-pattern",r);}},
        {"sampling","Area light and mirror sampling comparison",[](const std::string&){return make_sampling_scene();}},
        {"forest-v05","Forest v0.5 main",[](const std::string& r){return make_forest_v05(false,r,false);}},
        {"forest-v05-gray","Forest v0.5 gray",[](const std::string& r){return make_forest_v05(true,r,false);}},
        {"forest-v05-detail","Forest v0.5 detail",[](const std::string& r){return make_forest_v05(false,r,true);}},
    };
    return entries;
}
const SceneEntry& find_scene(std::string_view name) {
    for(const auto& entry:scene_catalog()) if(entry.name==name) return entry;
    throw std::runtime_error("Unknown scene: "+std::string(name));
}
World create_scene(std::string_view name,const std::string& resource_root) {
    return find_scene(name).factory(resource_root);
}
void write_scene_names(std::ostream& output) {
    bool first=true;
    for(const auto& entry:scene_catalog()) {
        if(!first) output<<'|';
        output<<entry.name;
        first=false;
    }
}
