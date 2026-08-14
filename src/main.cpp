#include <iostream>

// Ponto de entrada mínimo do IbiFX.
//
// Este executável existe apenas para validar a cadeia de build:
// source -> CMake -> compiler -> linker -> executable.
//
// Nenhum áudio, DSP ou engine é iniciado aqui ainda.
int main()
{
    std::cout << "IbiFX starting...\n";
    return 0;
}
