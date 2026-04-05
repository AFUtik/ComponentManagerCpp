#include <iostream>

#include "ResourceManager.hpp"

using SubId = u16;

struct Texture {
    uint32_t some_data;   
};

enum TextureRegistry {
    DefaultTexture
};

struct TextureManager : ResourceManager<Texture, TextureRegistry> {

};

static Texture create_texture(u32 some_data) {
    return Texture(some_data);
}

int main(int, char**)
{
    ResourceFactory<Texture, TextureRegistry, u32>::instance().register_factory(create_texture);
    auto& rm = TextureManager::instance();
    
    Resource texture = Resource<Texture, TextureRegistry>::load(TextureRegistry::DefaultTexture, 34u);
    std::cout << texture.get().some_data << std::endl;

    return 0;
}