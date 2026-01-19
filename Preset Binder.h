#pragma once
#include "pch.h"
#include <map>
#include <string>
#include <vector>

class PresetBinder : public BakkesMod::Plugin::BakkesModPlugin,
    public BakkesMod::Plugin::PluginSettingsWindow
{
public:
    void onLoad() override;
    void onUnload() override;
    void RenderSettings() override;
    std::string GetPluginName() override { return "Preset Binder"; }
    void SetImGuiContext(uintptr_t ctx) override { ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx)); }

private:
    std::map<int, std::string> presetBindings;
    int equippedPresetIndex = -1;

    char codeInputBuffer[512] = "";
    bool pluginEnabled = true;

    void LoadBindingsFromConfig();
    void SaveBindingsToConfig();
    void ApplyBakkesModPreset(const std::string& presetCode);

    void BindPreset(std::vector<std::string> args);
    void UnbindPreset(std::vector<std::string> args);
    void ListBindings(std::vector<std::string> args);
};
