#pragma once
namespace original_fov {
struct Fields { unsigned scoped,item,weapon,life,observer_mode,observer_target,resource_alive; };
struct Bindings { void* engine;void* entities;void** resource_slot;void* source_viewmodel; };
bool prepare(unsigned char* image,const Fields& fields) noexcept;
// Only original feature code performs FOV changes/calculations. The caller
// supplies a real current camera setup and verified public Source bindings.
float viewmodel(void* client_mode,const void* view,const Bindings& bindings);
void render(void* view,const Bindings& bindings);
}
