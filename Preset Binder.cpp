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

    // Hook preset equip event
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

    // Show existing bindings first
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Your Bindings");
    ImGui::Text("Manage your preset bindings below");
    ImGui::Spacing();

    if (presetBindings.empty()) {
        ImGui::TextDisabled("No bindings yet. Add one below!");
    }
    else {
        ImGui::BeginChild("BindingsList", ImVec2(0, 200), true);

        std::vector<int> toRemove;

        for (auto& [slot, code] : presetBindings) {
            ImGui::PushID(slot);

            // Highlight equipped preset in green
            if (slot == equippedPresetIndex) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.6f, 0.1f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.7f, 0.1f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.1f, 0.8f, 0.1f, 1.0f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.4f, 0.7f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.1f, 0.5f, 0.8f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.1f, 0.6f, 0.9f, 0.8f));
            }

            // Display as 1-indexed for user (slot 0 = "Preset 1")
            std::string headerText = "Preset " + std::to_string(slot + 1);
            if (slot == equippedPresetIndex) {
                headerText += " (Equipped)";
            }

            bool isOpen = ImGui::CollapsingHeader(headerText.c_str());
            ImGui::PopStyleColor(3);

            if (isOpen) {
                ImGui::Indent();

                ImGui::Text("BakkesMod Code:");
                ImGui::SameLine();

                std::string displayCode = code.length() > 40
                    ? code.substr(0, 37) + "..."
                    : code;
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", displayCode.c_str());

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", code.c_str());
                }

                ImGui::Spacing();

                if (ImGui::Button("Test")) {
                    ApplyBakkesModPreset(code);
                }
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                if (ImGui::Button("Remove")) {
                    toRemove.push_back(slot);
                }
                ImGui::PopStyleColor(2);

                ImGui::Unindent();
                ImGui::Spacing();
            }

            ImGui::PopID();
        }

        for (int slot : toRemove) {
            presetBindings.erase(slot);
        }
        if (!toRemove.empty()) {
            SaveBindingsToConfig();
        }

        ImGui::EndChild();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Add new binding section
    ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Add New Binding");
    ImGui::Spacing();

    static int newPresetSlot = 1;  // Start at 1 for user display
    ImGui::Text("Preset Number (1-50):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##newslot", &newPresetSlot);

    if (newPresetSlot < 1) newPresetSlot = 1;
    if (newPresetSlot > 50) newPresetSlot = 50;

    ImGui::Text("BakkesMod Code:");
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("##newcode", codeInputBuffer, sizeof(codeInputBuffer));

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    if (ImGui::Button("Add Binding", ImVec2(150, 0))) {
        if (strlen(codeInputBuffer) > 0) {
            // Convert from 1-indexed (user) to 0-indexed (internal)
            presetBindings[newPresetSlot - 1] = std::string(codeInputBuffer);
            SaveBindingsToConfig();
            codeInputBuffer[0] = '\0';
        }
    }
    ImGui::PopStyleColor(2);

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
            "3. Enter the preset number (1-50) matching your garage preset\n"
            "4. Paste the code and click 'Add Binding'\n"
            "5. When you switch to that garage preset, the Item Mod will auto-apply!\n\n"
            "Preset numbers are tied to order of creation for each preset.\n\n"
            "DISCLAIMER: The 'Test' button was used for me during development - will likely freeze/crash your game.\n\n"
            "Colors:\n"
            "Green = Currently equipped preset\n"
            "Blue = Preset with a binding"
        );
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Preset Binder v1.0.1 - made by shemmy.");
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
