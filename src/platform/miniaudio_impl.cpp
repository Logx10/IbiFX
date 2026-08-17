// Unidade de compilação dedicada à implementação do miniaudio.
//
// A biblioteca é distribuída como um único header de ~96 mil linhas. Definir
// MINIAUDIO_IMPLEMENTATION antes do include faz o pré-processador expandir o
// corpo das funções, e isso pode acontecer em EXATAMENTE um arquivo do
// projeto — em dois, o linker acharia cada símbolo duas vezes. É a mesma
// regra de uma definição que já apareceu no test_helpers.h.
//
// Este arquivo existe sozinho por dois motivos:
//
//   1. Ele é compilado SEM as flags de warning do IbiFX. Código de terceiros
//      não segue o nosso padrão, e 96 mil linhas sob -Wall -Wextra -Wpedantic
//      produziriam centenas de avisos que não podemos corrigir e que
//      esconderiam os nossos. A exceção está declarada no CMakeLists.
//
//   2. Ele isola a recompilação. Mexer no AudioDevice.cpp não obriga a
//      recompilar a biblioteca inteira, o que faria cada build custar
//      segundos a mais sem necessidade.

#define MINIAUDIO_IMPLEMENTATION

// Desliga o que o IbiFX não usa. Cada bloco removido é código que não é
// compilado, não entra no binário e não pode falhar.
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

#include "miniaudio.h"
