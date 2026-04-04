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

int main(int, char**)
{
    auto& rm = TextureManager::instance();

    GlobalResource<Texture> res = rm.create(TextureRegistry::DefaultTexture, Texture(34));

    Texture& number = rm.get_data(res);

    std::cout << number.some_data << std::endl;

    return 0;
}