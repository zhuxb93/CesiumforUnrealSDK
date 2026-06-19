#include "PoiWidget.h"

void UPoiWidget::SetPoiName(const FString& Name)
{
    if (PoiName)
    {
        PoiName->SetText(FText::FromString(Name));
    }
}