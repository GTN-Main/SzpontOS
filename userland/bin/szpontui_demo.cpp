// (C) Copyright by Szpont Industries. All rights reserved.
// SzpontUI Showcase Application — Modern C++ Desktop UI for Szpont Experience

#include <SzpontUI/SzpontUI.hpp>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    SzpontUI::Application app(argc, argv);

    // Create main application window
    auto win = std::make_shared<SzpontUI::Window>(
        SzpontUI::Size{700, 560},
        "SzpontUI Showcase — Szpont Experience"
    );

    // Root widget and vertical layout
    auto root = std::make_shared<SzpontUI::Widget>("Root");
    auto root_layout = std::make_shared<SzpontUI::VBoxLayout>();
    root_layout->set_padding(SzpontUI::Insets{18, 20, 18, 20});
    root_layout->set_spacing(14);
    root->set_layout(root_layout);

    // 1. Header Card / Banner
    auto header_card = std::make_shared<SzpontUI::Card>();
    auto header_layout = std::make_shared<SzpontUI::VBoxLayout>();
    header_layout->set_padding(SzpontUI::Insets{14, 18, 14, 18});
    header_layout->set_spacing(4);
    header_card->set_layout(header_layout);

    auto title_label = std::make_shared<SzpontUI::Label>(
        "SzpontUI v1.0 — Desktop Framework",
        SzpontUI::Font("Inter", 20, SzpontUI::FontWeight::Bold),
        SzpontUI::Color(91, 156, 248) // Accent blue
    );
    header_layout->add_widget(title_label);

    auto subtitle_label = std::make_shared<SzpontUI::Label>(
        "High-performance C++ UI stack: Pixman SIMD + FreeType 2 + HarfBuzz",
        SzpontUI::Font("Inter", 12, SzpontUI::FontWeight::Regular),
        SzpontUI::Color(148, 163, 184) // Slate 400
    );
    header_layout->add_widget(subtitle_label);
    root_layout->add_widget(header_card);

    // 2. Main Content Card
    auto content_card = std::make_shared<SzpontUI::Card>();
    auto content_layout = std::make_shared<SzpontUI::VBoxLayout>();
    content_layout->set_padding(SzpontUI::Insets{18, 18, 18, 18});
    content_layout->set_spacing(12);
    content_card->set_layout(content_layout);

    // Polish Diacritics Shaping test
    auto font_test_header = std::make_shared<SzpontUI::Label>(
        "Test renderowania i ksztaltowania tekstu (HarfBuzz + FreeType 2):",
        SzpontUI::Font("Inter", 12, SzpontUI::FontWeight::SemiBold),
        SzpontUI::Color(203, 213, 225)
    );
    content_layout->add_widget(font_test_header);

    auto polish_sample = std::make_shared<SzpontUI::Label>(
        "Zażółć gęślą jaźń — Piękna Polska Typografia!",
        SzpontUI::Font("Inter", 15, SzpontUI::FontWeight::Bold),
        SzpontUI::Color(56, 189, 248) // Sky 400
    );
    content_layout->add_widget(polish_sample);

    // Form row: TextBox + Primary Button
    auto input_row = std::make_shared<SzpontUI::HBoxLayout>();
    input_row->set_spacing(10);

    auto text_box = std::make_shared<SzpontUI::TextBox>("");
    text_box->set_placeholder("Wpisz wiadomosc i nacisnij Enter lub Wyślij...");
    input_row->add_widget(text_box, 1);

    auto send_btn = std::make_shared<SzpontUI::Button>("Wyślij");
    send_btn->set_style(SzpontUI::ButtonStyle::Primary);
    input_row->add_widget(send_btn);
    content_layout->add_item(input_row);

    // Controls row: Counter Button + Reset Button + CheckBox
    auto btn_row = std::make_shared<SzpontUI::HBoxLayout>();
    btn_row->set_spacing(10);

    auto counter_btn = std::make_shared<SzpontUI::Button>("Licznik: 0");
    btn_row->add_widget(counter_btn);

    auto reset_btn = std::make_shared<SzpontUI::Button>("Resetuj");
    reset_btn->set_style(SzpontUI::ButtonStyle::Danger);
    btn_row->add_widget(reset_btn);

    auto checkbox = std::make_shared<SzpontUI::CheckBox>("Akceleracja sprzetowa", true);
    btn_row->add_widget(checkbox);
    content_layout->add_item(btn_row);

    // Slider & Progress Bar row
    auto slider_label = std::make_shared<SzpontUI::Label>(
        "Poziom bufora renderowania: 50%",
        SzpontUI::Font("Inter", 12, SzpontUI::FontWeight::Regular),
        SzpontUI::Color(203, 213, 225)
    );
    content_layout->add_widget(slider_label);

    auto progress_bar = std::make_shared<SzpontUI::ProgressBar>(0.5f);
    content_layout->add_widget(progress_bar);

    auto slider = std::make_shared<SzpontUI::Slider>(0.0f, 1.0f, 0.5f);
    content_layout->add_widget(slider);

    root_layout->add_widget(content_card, 1);

    // 3. Footer Status Card
    auto footer_card = std::make_shared<SzpontUI::Card>();
    auto footer_layout = std::make_shared<SzpontUI::HBoxLayout>();
    footer_layout->set_padding(SzpontUI::Insets{8, 14, 8, 14});
    footer_card->set_layout(footer_layout);

    auto status_label = std::make_shared<SzpontUI::Label>(
        "Stan: SzpontUI aktywny. Zdarzenia X11 i timerfd aktywne.",
        SzpontUI::Font("Inter", 11, SzpontUI::FontWeight::Regular),
        SzpontUI::Color(148, 163, 184)
    );
    footer_layout->add_widget(status_label, 1);
    root_layout->add_widget(footer_card);

    // Signal / Slot Wiring
    static int click_count = 0;
    counter_btn->on_click.connect([counter_btn, status_label]() {
        click_count++;
        counter_btn->set_text("Licznik: " + std::to_string(click_count));
        status_label->set_text("Kliknieto licznik (razem: " + std::to_string(click_count) + ")");
    });

    reset_btn->on_click.connect([counter_btn, status_label]() {
        click_count = 0;
        counter_btn->set_text("Licznik: 0");
        status_label->set_text("Zresetowano licznik.");
    });

    auto submit_action = [text_box, status_label]() {
        std::string msg = text_box->text();
        if (msg.empty()) {
            status_label->set_text("Pole tekstowe jest puste.");
        } else {
            status_label->set_text("Wprowadzono tekst: \"" + msg + "\"");
        }
    };

    send_btn->on_click.connect(submit_action);
    text_box->on_return_pressed.connect([submit_action](const std::string &) {
        submit_action();
    });

    checkbox->on_toggled.connect([status_label](bool checked) {
        status_label->set_text(checked ? "Wlaczono akceleracje sprzetowa." : "Wylaczono akceleracje sprzetowa.");
    });

    slider->on_value_changed.connect([progress_bar, slider_label](float val) {
        progress_bar->set_value(val);
        int pct = static_cast<int>(val * 100.0f + 0.5f);
        slider_label->set_text("Poziom bufora renderowania: " + std::to_string(pct) + "%");
    });

    // Attach root to window and show
    win->set_root_widget(root);
    win->show();

    return app.exec();
}
