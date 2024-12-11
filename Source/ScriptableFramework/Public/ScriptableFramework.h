// Copyright Kirzo. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FScriptableFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
