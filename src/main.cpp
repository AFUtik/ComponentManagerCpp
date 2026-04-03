#include <iostream>

#include "ResourceManager.hpp"

using SubId = u16;

struct TextureManager : ResourceManager<uint32_t> {

};

int main(int, char**)
{
    auto& rm = TextureManager::instance();
    GlobalResource<uint32_t> res = rm.create("number:ten", 10); // global resource //

    auto number = rm.get_data(res.id);

    std::cout << number << std::endl;

    return 0;
}