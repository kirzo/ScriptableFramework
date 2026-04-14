// Copyright 2026 kirzo

#pragma once

#include "Modules/ModuleManager.h"

class FScriptableFrameworkAIModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};