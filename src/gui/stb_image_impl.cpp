// A implementação do stb_image (third_party/stb), compilada uma vez só, no
// alvo ibifx_stb — sem as flags de aviso do IbiFX, como o miniaudio.
//
// Só PNG: é o formato dos assets (assets/gear, assets/brand). Desligar os
// outros decodificadores deixa o binário menor e a superfície de ataque
// também — o app só abre imagens que vêm com ele.
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
