#pragma once
namespace original_movement {
struct Fields { unsigned water,flags,velocity,ground; };
// Copies guarded original code and repairs only verified external operands.
// No original entry, constructors or full feature coordinator are called.
bool prepare(unsigned char* image,const Fields& fields) noexcept;
void run(void* command,void* engine,void* entities);
}
