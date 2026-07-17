// We Compile TinyGLTF's glTF parser exactly once. TinyGLTF uses stb to decode model
// images, but the framework already provides stb's implementation. Therefore,
// do not define STB_IMAGE_IMPLEMENTATION here; doing so would create duplicate
// stb symbols when the engine and framework are linked together.
#if defined(__clang__)
// tiny_gltf.h triggers this warning in bundled nlohmann/json code.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-literal-operator"
#endif
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
