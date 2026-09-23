#pragma once

// SzpontUI — Modern C++ UI Framework for Szpont Experience (SzpontOS)
// (C) Copyright by Szpont Industries. All rights reserved.

// Core
#include <SzpontUI/Core/Geometry.hpp>
#include <SzpontUI/Core/Color.hpp>
#include <SzpontUI/Core/Signal.hpp>
#include <SzpontUI/Core/String.hpp>
#include <SzpontUI/Core/Object.hpp>

// Graphics & Typography
#include <SzpontUI/Gfx/BitmapSurface.hpp>
#include <SzpontUI/Gfx/Painter.hpp>
#include <SzpontUI/Font/Font.hpp>
#include <SzpontUI/Font/FontDatabase.hpp>
#include <SzpontUI/Font/TextShaper.hpp>
#include <SzpontUI/Font/GlyphCache.hpp>

// Platform & Events
#include <SzpontUI/Platform/Event.hpp>
#include <SzpontUI/Platform/PlatformBackend.hpp>
#include <SzpontUI/Platform/X11Backend.hpp>

// Layout
#include <SzpontUI/Layout/Layout.hpp>
#include <SzpontUI/Layout/BoxLayout.hpp>
#include <SzpontUI/Layout/Spacer.hpp>

// Theme
#include <SzpontUI/Theme/Theme.hpp>
#include <SzpontUI/Theme/SzpontTheme.hpp>

// Widgets
#include <SzpontUI/Widgets/Widget.hpp>
#include <SzpontUI/Widgets/Window.hpp>
#include <SzpontUI/Widgets/Label.hpp>
#include <SzpontUI/Widgets/Button.hpp>
#include <SzpontUI/Widgets/TextBox.hpp>
#include <SzpontUI/Widgets/CheckBox.hpp>
#include <SzpontUI/Widgets/ProgressBar.hpp>
#include <SzpontUI/Widgets/Slider.hpp>
#include <SzpontUI/Widgets/Card.hpp>

// Application Runtime
#include <SzpontUI/App/Application.hpp>
#include <SzpontUI/App/Timer.hpp>
