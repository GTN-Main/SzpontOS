#include <SzpontUI/Theme/Theme.hpp>
#include <SzpontUI/Theme/SzpontTheme.hpp>

namespace SzpontUI {

ThemeManager &ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    theme_ = std::make_shared<SzpontTheme>();
}

} // namespace SzpontUI
