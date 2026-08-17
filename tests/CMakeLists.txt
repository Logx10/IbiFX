# Um teste é apenas mais um executável.
#
# Não há mágica: ele tem seu próprio main(), é compilado como qualquer
# outro programa e pode ser executado direto no terminal.
#
# UM EXECUTÁVEL POR MÓDULO TESTADO
# Cada bloco abaixo é independente. Os dois .cpp de teste NÃO podem virar
# fontes de um mesmo executável: cada um tem seu próprio main(), sua própria
# variável `failures` e suas próprias check()/checkClose(). Juntá-los faria o
# linker encontrar cada um desses símbolos duas vezes e recusar o link.
#
# Manter separado também é melhor na prática: o CTest passa a reportar
# "gain_processor" e "clipper" como testes distintos, e uma falha aponta
# direto para o módulo culpado.

add_executable(test_gain_processor
    test_gain_processor.cpp
)

target_compile_options(test_gain_processor PRIVATE ${IBIFX_WARNINGS})

# Aqui está o motivo de existir a ibifx_core: o teste precisa do mesmo
# GainProcessor que o executável usa — não de uma cópia dele.
target_link_libraries(test_gain_processor PRIVATE ibifx_core)

add_executable(test_clipper
    test_clipper.cpp
)

target_compile_options(test_clipper PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_clipper PRIVATE ibifx_core)

add_executable(test_soft_clipper
    test_soft_clipper.cpp
)

target_compile_options(test_soft_clipper PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_soft_clipper PRIVATE ibifx_core)

add_executable(test_audio_module
    test_audio_module.cpp
)

target_compile_options(test_audio_module PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_audio_module PRIVATE ibifx_core)

add_executable(test_module_chain
    test_module_chain.cpp
)

target_compile_options(test_module_chain PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_module_chain PRIVATE ibifx_core)

add_executable(test_parameter
    test_parameter.cpp
)

target_compile_options(test_parameter PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_parameter PRIVATE ibifx_core)

add_executable(test_delay
    test_delay.cpp
)

target_compile_options(test_delay PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_delay PRIVATE ibifx_core)

add_executable(test_smoothed_value
    test_smoothed_value.cpp
)

target_compile_options(test_smoothed_value PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_smoothed_value PRIVATE ibifx_core)

add_executable(test_wav_file
    test_wav_file.cpp
)

target_compile_options(test_wav_file PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_wav_file PRIVATE ibifx_core)

add_executable(test_offline
    test_offline.cpp
)

target_compile_options(test_offline PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_offline PRIVATE ibifx_core)

add_executable(test_audio_graph
    test_audio_graph.cpp
)

target_compile_options(test_audio_graph PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_audio_graph PRIVATE ibifx_core)

# Este teste cria threads de verdade, entao precisa da biblioteca de threads
# do sistema. Threads::Threads e o alvo portavel do CMake para isso: resolve
# para -pthread no Linux, e para nada no macOS e no Windows, onde o suporte
# ja vem embutido.
add_executable(test_command_queue
    test_command_queue.cpp
)

target_compile_options(test_command_queue PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_command_queue PRIVATE ibifx_core Threads::Threads)

# Este e o unico teste que liga na camada de plataforma. Ele usa o backend
# nulo do miniaudio, entao roda sem placa de som e sem permissao de microfone.
add_executable(test_live_engine
    test_live_engine.cpp
)

target_compile_options(test_live_engine PRIVATE ${IBIFX_WARNINGS})
target_link_libraries(test_live_engine PRIVATE ibifx_platform Threads::Threads)

# Registra cada executável como um teste do CTest.
#
#   NAME     como o teste aparece nos relatórios
#   COMMAND  o que executar
#
# COMO O CTEST SABE SE PASSOU
# Ele não lê a saída do programa. Ele olha o CÓDIGO DE SAÍDA:
#
#   return 0  -> passou
#   return ≠0 -> falhou
#
# É a mesma convenção que o shell usa desde sempre, e é por isso que o
# nosso main() de teste termina com `return failures == 0 ? 0 : 1`.
add_test(NAME gain_processor COMMAND test_gain_processor)
add_test(NAME clipper COMMAND test_clipper)
add_test(NAME soft_clipper COMMAND test_soft_clipper)
add_test(NAME audio_module COMMAND test_audio_module)
add_test(NAME module_chain COMMAND test_module_chain)
add_test(NAME parameter COMMAND test_parameter)
add_test(NAME delay COMMAND test_delay)
add_test(NAME smoothed_value COMMAND test_smoothed_value)
add_test(NAME wav_file COMMAND test_wav_file)
add_test(NAME offline COMMAND test_offline)
add_test(NAME audio_graph COMMAND test_audio_graph)
add_test(NAME command_queue COMMAND test_command_queue)
add_test(NAME live_engine COMMAND test_live_engine)
