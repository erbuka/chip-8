#pragma once

#ifdef DEBUG 
//#define C8_INFO(...) { spdlog::info(__VA_ARGS__); }
#define C8_INFO(...) 
#else
#define C8_INFO(...)
#endif

#define C8_ERRO(...) { spdlog::error(__VA_ARGS__); }
#define C8_UNKN(instr, addr) C8_INFO("Unknown instruction {0:x} at address {1:x}", instr, addr - 2)

#define GL_CALL(x) { x; auto err = glGetError(); if(err != GL_NO_ERROR) C8_ERRO("GL error at {0}:{1} with code: {2}", __FILE__, __LINE__, err); }


