// Copyright Kirzo. All Rights Reserved.

#include "ScriptableFramework.h"

#define LOCTEXT_NAMESPACE "FScriptableFrameworkModule"

void FScriptableFrameworkModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FScriptableFrameworkModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FScriptableFrameworkModule, ScriptableFramework)