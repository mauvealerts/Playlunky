#include "script_manager.h"

#include "log.h"
#include "util/algorithms.h"

#include <spel2.h>

#include <imgui.h>

bool ScriptManager::RegisterModWithScript(std::string_view mod_name, const std::filesystem::path& main_path, bool enabled) {
	if (algo::contains(mMods, &RegisteredMainScript::ModName, mod_name)) {
		return false;
	}
	mMods.push_back(RegisteredMainScript{ .ModName{ std::string{ mod_name } }, .MainPath{ main_path }, .Enabled{ enabled }, .ScriptEnabled{ enabled } });
	return true;
}

void ScriptManager::CommitScripts() {
	for (RegisteredMainScript& mod : mMods) {
		const std::string path_string = mod.MainPath.string();
		mod.Script = CreateScript(path_string.c_str(), mod.ScriptEnabled);
		mod.TestScriptResult();
	}
}
void ScriptManager::RefreshScripts() {
	for (RegisteredMainScript& mod : mMods) {
		const std::string path_string = mod.MainPath.string();
		SpelunkyScipt_ReloadScript(mod.Script, path_string.c_str());
		mod.LastResult.clear();
	}
}
void ScriptManager::Update() {
	for (RegisteredMainScript& mod : mMods) {
		if (mod.Script) {
			SpelunkyScript_Update(mod.Script);
			mod.TestScriptResult();

			const std::size_t num_messages = SpelunkyScript_GetNumMessages(mod.Script);
			for (std::size_t i = 0; i < num_messages; i++) {
				SpelunkyScriptMessage message = SpelunkyScript_GetMessage(mod.Script, i);
				if (message.Message != nullptr && message.TimeMilliSecond > mod.MessageTime) {
					LogInfoScreen("[{}]: {}", mod.ModName, message.Message);
				}
			}
			mod.MessageTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}
	}
}
void ScriptManager::Draw() {
	if (mMods.empty())
		return;

	ImGuiIO& io = ImGui::GetIO();

	if (SpelunkyState_GetScreen() == SpelunkyScreen::Online)
	{
		ImGui::SetNextWindowSize({ -1, -1 });
		ImGui::Begin(
			"Online Warning Overlay",
			nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
		{
			const int num_colors = 64;
			const float frequency = 10.0f / num_colors;
			for (int i = 0; i < num_colors; ++i)
			{
				const float red = std::sin(frequency * i + 0) * 0.5f + 0.5f;
				const float green = std::sin(frequency * i + 2) * 0.5f + 0.5f;
				const float blue = std::sin(frequency * i + 4) * 0.5f + 0.5f;

				ImGui::TextColored(ImVec4(red, green, blue, 1.0f), "Do not use script mods online! Your game will not work! ");
				for (int j = 0; j < 6; j++)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(red, green, blue, 1.0f), "Do not use script mods online! Your game will not work! ");
				}
			}
		}
		ImGui::SetWindowPos({ ImGui::GetIO().DisplaySize.x / 2 - ImGui::GetWindowWidth() / 2, ImGui::GetIO().DisplaySize.y / 2 - ImGui::GetWindowHeight() / 2 }, ImGuiCond_Always);
		ImGui::End();
	}

	if (mForceShowOptions || SpelunkyState_GetScreen() == SpelunkyScreen::Menu) {
		if (!GetShowCursor()) {
			SetShowCursor(true);
		}

		ImGui::SetNextWindowSize({ io.DisplaySize.x / 4, io.DisplaySize.y });
		ImGui::SetNextWindowPos({ io.DisplaySize.x * 3 / 4, 0 });
		ImGui::Begin(
			"Mod Options",
			NULL,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

		ImGui::TextUnformatted("Mod Options");

		for (RegisteredMainScript& mod : mMods) {
			if (mod.Script && mod.Enabled) {
				ImGui::Separator();
				if (ImGui::Checkbox(mod.ModName.c_str(), &mod.ScriptEnabled)) {
					SpelunkyScipt_SetEnabled(mod.Script, mod.ScriptEnabled);
				}
				SpelunkyScript_DrawOptions(mod.Script);
			}
		}

		ImGui::End();
	}
	else if (GetShowCursor()) {
		SetShowCursor(false);
	}

	ImGui::SetNextWindowSize(io.DisplaySize);
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::Begin(
		"Clickhandler",
		NULL,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);

	ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
	for (RegisteredMainScript& mod : mMods) {
		if (mod.Script) {
			SpelunkyScript_Draw(mod.Script, draw_list);
		}
	}

	ImGui::End();
}

void ScriptManager::RegisteredMainScript::TestScriptResult() {
	using namespace std::literals::string_view_literals;
	if (const char* res = SpelunkyScript_GetResult(Script)) {
		if (res != "Got metadata"sv && res != "OK"sv && res != LastResult) {
			LogError("Lua Error:\n\tMod: {}\n\tError: {}", ModName, res);
			LastResult = res;
		}
	}
}
