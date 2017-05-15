#include <C2Component.h>

#define C2_IMPLEMENTATION

namespace android {

status_t C2ComponentStore::createComponent(C2String, std::shared_ptr<C2Component>* const) {

    return C2_NOT_IMPLEMENTED;
}

status_t C2ComponentStore::createInterface(C2String, std::shared_ptr<C2ComponentInterface>* const) {

    return C2_NOT_IMPLEMENTED;
}

std::vector<std::unique_ptr<const C2ComponentInfo>> C2ComponentStore::getComponents() {

    return std::vector<std::unique_ptr<const C2ComponentInfo>>();
}

status_t C2ComponentStore::copyBuffer(std::shared_ptr<C2GraphicBuffer>, std::shared_ptr<C2GraphicBuffer>) {

    return C2_NOT_IMPLEMENTED;
}

}
