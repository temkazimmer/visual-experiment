#include "Modules/ModuleManager.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "VisexClothNodes.h"

class FVisexEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVisexHeightWeightMapNode);
	}
};

IMPLEMENT_MODULE(FVisexEditorModule, visexEditor);
