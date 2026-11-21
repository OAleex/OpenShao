#pragma once

#include <cstdint>
#include <string>
#include <string_view>

struct X86Context;

namespace openshao::campaign_moves {

bool ExtraFighter(uint32_t fighter);
uint32_t RootFor(uint32_t fighter);
bool ReferenceNode(uint32_t node);
bool ReferenceCaption(uint32_t label, std::string& text);
uint32_t ReferenceTitleSource(uint32_t label);
std::string_view ReferenceTranslationKey(uint32_t label);
bool LocalizedReferenceTitle(uint32_t address, std::string& title);
bool CorrectCaption(uint32_t label, uint32_t address, std::string& corrected);
bool FitReferenceList(X86Context& ctx);
void FitReferenceDetails(X86Context& ctx, uint8_t* base, void (*draw)(X86Context&, uint8_t*));
void ResetImportedState();

class ScopedRootView {
public:
    explicit ScopedRootView(uint32_t fighter);
    ~ScopedRootView();
    ScopedRootView(const ScopedRootView&) = delete;
    ScopedRootView& operator=(const ScopedRootView&) = delete;
private:
    uint32_t slot_ = 0, previous_root_ = 0, label_ = 0, previous_label_ = 0;
};

}
