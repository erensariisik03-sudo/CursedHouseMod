#pragma once

// The actual implementation is provided by the runtime Substrate environment.
// This declaration is enough to link this shared library without bundling
// Substrate itself into the project.
extern "C" void MSHookFunction(void* symbol, void* replace, void** result);
