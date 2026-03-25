#include "Utils/UI.h"

#include "API/PrismaUI_API.h"

namespace UI
{
static PRISMA_UI_API::IVPrismaUI2* prisma = nullptr;
static PrismaView view                    = 0;

void Initialize()
{
  constexpr std::string_view htmlPath = "SexLabNG/index.html";

  prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
  if (!prisma) {
    logger::error("[SexLab NG] Failed to acquire Prisma UI API.");
    return;
  }
  view = prisma->CreateView(htmlPath.data(), [](PrismaView view) {
    prisma->Hide(view);
    logger::info("UI Ready");
  });
}
}  // namespace UI