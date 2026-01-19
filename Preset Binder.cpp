#include "pch.h"
#include "Preset Binder.h"
#include <fstream>
#include <sstream>

BAKKESMOD_PLUGIN(PresetBinder, "Preset Binder", "1.0", PLUGINTYPE_FREEPLAY)

void PresetBinder::onLoad()
{
    LoadBindingsFromConfig();

    // Register CVars
    cvarManager->registerCvar("pb_enabled", "1", "Enable Preset Binder", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
        pluginEnabled = cvar.getBoolValue();
            });

    // Register commands
    cvarManager->registerNotifier("pb_bind", [this](std::vector<std::string> args) {
        BindPreset(args);
        }, "Bind a BakkesMod preset code to an RL preset slot", PERMISSION_ALL);

    cvarManager->registerNotifier("pb_unbind", [this](std::vector<std::string> args) {
        UnbindPreset(args);
        }, "Unbind an RL preset slot", PERMISSION_ALL);

    cvarManager->registerNotifier("pb_list", [this](std::vector<std::string> args) {
        ListBindings(args);
        }, "List all bindings", PERMISSION_ALL);

    // Register apply commands
    for (int i = 1; i <= 10; i++) {
        std::string commandName = "pb_apply_" + std::to_string(i);
        cvarManager->registerNotifier(commandName, [this, i](std::vector<std::string> args) {
            auto it = presetBindings.find(i);
            if (it != presetBindings.end()) {
                ApplyBakkesModPreset(it->second);
            }
            }, "Apply BakkesMod preset for slot " + std::to_string(i), PERMISSION_ALL);
    }

    cvarManager->registerNotifier("pb_apply", [this](std::vector<std::string> args) {
        if (args.size() < 2) return;
        try {
            int slot = std::stoi(args[1]);
            auto it = presetBindings.find(slot);
            if (it != presetBindings.end()) {
                ApplyBakkesModPreset(it->second);
            }
        }
        catch (...) {}
        }, "Apply BakkesMod preset for a specific slot", PERMISSION_ALL);

    // Hook preset equip event - SAFE VERSION
    cvarManager->log("Hooking preset change event...");

    gameWrapper->HookEventWithCallerPost<ActorWrapper>(
        "Function TAGame.GFxData_LoadoutSets_TA.EquipPreset",
        [this](ActorWrapper caller, void* params, std::string eventName) {
            if (!pluginEnabled) return;
            if (!params) return;

            struct EquipPresetParams {
                int Index;
            };

            auto* presetParams = reinterpret_cast<EquipPresetParams*>(params);
            int presetIndex = presetParams->Index;

            // Validate index
            if (presetIndex < 0 || presetIndex >= maxPresets) return;

            equippedPresetIndex = presetIndex;

            // Apply binding if exists
            auto it = presetBindings.find(presetIndex);
            if (it != presetBindings.end()) {
                ApplyBakkesModPreset(it->second);
            }
        }
    );

    cvarManager->log("Preset Binder loaded!");
}

void PresetBinder::onUnload()
{
    SaveBindingsToConfig();
}

void PresetBinder::RenderSettings()
{
    // Header
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Preset Binder");
    ImGui::Text("Bind BakkesMod item codes to garage presets");
    ImGui::Separator();
    ImGui::Spacing();

    // Enable toggle
    if (ImGui::Checkbox("Enable automatic preset switching", &pluginEnabled))
    {
        cvarManager->getCvar("pb_enabled").setValue(pluginEnabled);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Preset list
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Garage Presets");
    ImGui::Text("Expand a preset to add or manage its binding");
    ImGui::Spacing();

    ImGui::BeginChild("PresetsList", ImVec2(0, 400), true);

    for (int i = 0; i < maxPresets; i++) {
        ImGui::PushID(i);

        bool hasBinding = (presetBindings.find(i) != presetBindings.end());

        // Highlight equipped preset in green
        if (i == equippedPresetIndex) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.6f, 0.1f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.7f, 0.1f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.1f, 0.8f, 0.1f, 1.0f));
        }
        // Highlight presets with bindings in blue
        else if (hasBinding) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.4f, 0.7f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.5f, 0.8f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.1f, 0.6f, 0.9f, 0.8f));
        }

        // Preset header
        std::string headerText = "Preset " + std::to_string(i + 1);
        if (i == equippedPresetIndex) {
            headerText += " (Equipped)";
        }
        if (hasBinding) {
            headerText += " [Bound]";
        }

        bool isOpen = ImGui::CollapsingHeader(headerText.c_str());

        if (i == equippedPresetIndex || hasBinding) {
            ImGui::PopStyleColor(3);
        }

        if (isOpen) {
            ImGui::Indent();

            auto it = presetBindings.find(i);

            if (hasBinding) {
                // Show existing binding
                ImGui::Text("BakkesMod Code:");
                ImGui::SameLine();

                std::string displayCode = it->second.length() > 40
                    ? it->second.substr(0, 37) + "..."
                    : it->second;
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", displayCode.c_str());

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", it->second.c_str());
                }

                ImGui::Spacing();

                // Test and remove buttons
                if (ImGui::Button("Test")) {
                    ApplyBakkesModPreset(it->second);
                }
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Remove Binding")) {
                    presetBindings.erase(i);
                    SaveBindingsToConfig();
                }
                ImGui::PopStyleColor(2);
            }
            else {
                // Add new binding
                ImGui::Text("Paste your BakkesMod code here:");
                ImGui::SetNextItemWidth(-1);
                std::string inputId = "##code" + std::to_string(i);
                ImGui::InputText(inputId.c_str(), codeInputBuffer, sizeof(codeInputBuffer));

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                if (ImGui::Button("Add Binding")) {
                    if (strlen(codeInputBuffer) > 0) {
                        presetBindings[i] = std::string(codeInputBuffer);
                        SaveBindingsToConfig();
                        codeInputBuffer[0] = '\0';
                    }
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::Unindent();
            ImGui::Spacing();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Statistics
    ImGui::Text("Total bindings: %d", (int)presetBindings.size());
    if (equippedPresetIndex >= 0) {
        ImGui::Text("Currently equipped: Preset %d", equippedPresetIndex + 1);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Help section
    if (ImGui::CollapsingHeader("Help")) {
        ImGui::Indent();
        ImGui::TextWrapped(
            "How to use:\n"
            "1. Create a custom preset in BakkesMod's Item Mod menu\n"
            "2. Copy the preset code\n"
            "3. Expand the garage preset number you want to bind\n"
            "4. Paste the code and click 'Add Binding'\n"
            "5. When you switch to that garage preset, the Item Mod will auto-apply!\n\n"
            "Colours:\n"
            "Green = Currently equipped preset\n"
            "Blue = Preset with a binding\n\n"
            "Tip: You can also use console commands like 'pb_apply_1' to manually apply bindings."
        );
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Preset Binder v1.0 - made by shemmy.");
}

void PresetBinder::LoadBindingsFromConfig()
{
    std::ifstream file(gameWrapper->GetDataFolder() / "preset_bindings.cfg");
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        int slot;
        std::string code;
        if (iss >> slot)
        {
            std::getline(iss >> std::ws, code);
            if (!code.empty())
            {
                presetBindings[slot] = code;
            }
        }
    }
    file.close();
}

void PresetBinder::SaveBindingsToConfig()
{
    std::ofstream file(gameWrapper->GetDataFolder() / "preset_bindings.cfg");
    if (!file.is_open()) return;

    for (const auto& [slot, code] : presetBindings)
    {
        file << slot << " " << code << "\n";
    }
    file.close();
}

void PresetBinder::ApplyBakkesModPreset(const std::string& presetCode)
{
    if (!cvarManager) return;

    cvarManager->executeCommand("cl_itemmod_code " + presetCode);
    cvarManager->executeCommand("cl_itemmod_enabled 1");
}

void PresetBinder::BindPreset(std::vector<std::string> args)
{
    if (args.size() < 3) return;

    try {
        int slot = std::stoi(args[1]);
        presetBindings[slot] = args[2];
        SaveBindingsToConfig();
    }
    catch (...) {}
}

void PresetBinder::UnbindPreset(std::vector<std::string> args)
{
    if (args.size() < 2) return;

    try {
        int slot = std::stoi(args[1]);
        if (presetBindings.erase(slot)) {
            SaveBindingsToConfig();
        }
    }
    catch (...) {}
}

void PresetBinder::ListBindings(std::vector<std::string> args)
{
    if (presetBindings.empty()) {
        cvarManager->log("No bindings.");
        return;
    }
    for (const auto& [slot, code] : presetBindings)
        cvarManager->log("Slot " + std::to_string(slot) + " -> " + code);
}