#pragma once
#include <atomic>
#include <cstdint>
namespace sma3 {
inline std::atomic<int> language{0}; // 0 English, 1 es-419, 2 pt-BR
int localized_read(uint32_t pc,uint32_t address,uint32_t width,uint32_t original,uint32_t* output);
}
