#pragma once

#ifdef DEBUG 
//#define C8_INFO(...) { spdlog::info(__VA_ARGS__); }
#define C8_INFO(...) 
#else
#define C8_INFO(...)
#endif

#define C8_ERRO(...) { spdlog::error(__VA_ARGS__); }
#define C8_UNKN(instr, addr) C8_INFO("Unknown instruction {0:x} at address {1:x}", instr, addr - 2)

