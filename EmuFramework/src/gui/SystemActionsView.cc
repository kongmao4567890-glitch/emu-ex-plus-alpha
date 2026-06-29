/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <emuframework/SystemActionsView.hh>
#include "InputOverridesView.hh"
#include "AutosaveSlotView.hh"
#include "ResetAlertView.hh"
#include <emuframework/EmuApp.hh>
#include <emuframework/Cheats.hh>
#include <emuframework/StateSlotView.hh>
import imagine;

namespace EmuEx
{

constexpr SystemLogger log{"SystemActionsView"};

static auto autoSaveName(EmuApp &app)
{
	return std::format("Autosave Slot ({})", app.autosaveManager.slotFullName());
}

static std::string saveAutosaveName(EmuApp &app)
{
	auto &autosaveManager = app.autosaveManager;
	if(!autosaveManager.timerFrequency().count())
		return "保存自动存档";
	return std::format("Save Autosave State (Timer In {:%M:%S})",
		duration_cast<Seconds>(autosaveManager.saveTimer.nextFireDuration()));
}

SystemActionsView::SystemActionsView(ViewAttachParams attach, bool customMenu):
	TableView{"系统操作", attach, item},
	cheats
	{
		"金手指", attach,
		[this](const Input::Event &e)
		{
			if(system().hasContent())
			{
				pushAndShow(makeView<CheatsView>(), e);
			}
		}
	},
	reset
	{
		"重置", attach,
		[this](const Input::Event &e)
		{
			if(!system().hasContent())
				return;
			pushAndShowModal(resetAlertView(attachParams(), app()), e);
		}
	},
	autosaveSlot
	{
		autoSaveName(app()), attach,
		[this](const Input::Event &e) { pushAndShow(makeView<AutosaveSlotView>(), e); }
	},
	autosaveNow
	{
		saveAutosaveName(app()), attach,
		[this](TextMenuItem &item, const Input::Event &e)
		{
			if(!item.active())
				return;
			pushAndShowModal(makeView<YesNoAlertView>("确定保存状态？",
				YesNoAlertView::Delegates
				{
					.onYes = [this]
					{
						if(app().autosaveManager.save(AutosaveActionSource::Manual))
							app().showEmulation();
					}
				}), e);
		}
	},
	revertAutosave
	{
		"读取自动存档", attach,
		[this](TextMenuItem &item, const Input::Event &e)
		{
			if(!item.active())
				return;
			auto saveTime = app().autosaveManager.stateTimeAsString();
			if(saveTime.empty())
			{
				app().postMessage("无已保存的状态");
				return;
			}
			pushAndShowModal(makeView<YesNoAlertView>(std::format("Really load state from: {}?", saveTime),
				YesNoAlertView::Delegates
				{
					.onYes = [this]
					{
						if(app().autosaveManager.load(AutosaveActionSource::Manual))
							app().showEmulation();
					}
				}), e);
		}
	},
	stateSlot
	{
		"手动存档", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<StateSlotView>(), e);
		}
	},
	inputOverrides
	{
		"输入覆盖", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<InputOverridesView>(app().inputManager), e);
		}
	},
	addLauncherIcon
	{
		"添加内容快捷方式到桌面", attach,
		[this](const Input::Event &e)
		{
			if(!system().hasContent())
				return;
			if(system().contentDirectory().empty())
			{
				// shortcuts to bundled games not yet supported
				return;
			}
			pushAndShowNewCollectValueInputView<const char*>(attachParams(), e, "快捷方式名称", system().contentDisplayName(),
				[this](CollectTextInputView &, auto str)
				{
					appContext().addLauncherIcon(str, system().contentLocation());
					app().postMessage(2, false, std::format("Added shortcut:\n{}", str));
					return true;
				});
		}
	},
	screenshot
	{
		"截取下一帧", attach,
		[this](const Input::Event &e)
		{
			if(!system().hasContent())
				return;
			auto pathName = appContext().fileUriDisplayName(app().screenshotDirectory());
			if(pathName.empty())
			{
				app().postMessage("保存路径无效");
				return;
			}
			pushAndShowModal(makeView<YesNoAlertView>(std::format("Save screenshot to folder {}?", pathName),
				YesNoAlertView::Delegates
				{
					.onYes = [this]
					{
						app().video.takeGameScreenshot();
						system().runFrame({}, &app().video, nullptr);
					}
				}), e);
		}
	},
	resetSessionOptions
	{
		"重置已保存选项", attach,
		[this](const Input::Event &e)
		{
			if(!app().hasSavedSessionOptions())
				return;
			pushAndShowModal(makeView<YesNoAlertView>(
				"Reset saved options for the currently running system to defaults? Some options only take effect next time the system loads.",
				YesNoAlertView::Delegates
				{
					.onYes = [this]
					{
						resetSessionOptions.setActive(false);
						app().deleteSessionOptions();
					}
				}), e);
		}
	},
	close
	{
		"关闭内容", attach,
		[this](const Input::Event &e)
		{
			pushAndShowModal(app().makeCloseContentView(), e);
		}
	}
{
	if(!customMenu)
	{
		loadStandardItems();
	}
}

void SystemActionsView::onShow()
{
	if(app().viewController().isShowingEmulation())
		return;
	TableView::onShow();
	log.info("refreshing action menu state");
	assume(system().hasContent());
	autosaveSlot.compile(autoSaveName(app()));
	autosaveNow.compile(saveAutosaveName(app()));
	autosaveNow.setActive(app().autosaveManager.slotName() != noAutosaveName);
	revertAutosave.setActive(app().autosaveManager.slotName() != noAutosaveName);
	resetSessionOptions.setActive(app().hasSavedSessionOptions());
}

void SystemActionsView::loadStandardItems()
{
	if(AppMeta::hasCheats)
	{
		item.emplace_back(&cheats);
	}
	item.emplace_back(&reset);
	item.emplace_back(&autosaveSlot);
	item.emplace_back(&revertAutosave);
	item.emplace_back(&autosaveNow);
	item.emplace_back(&stateSlot);
	item.emplace_back(&inputOverrides);
	if(used(addLauncherIcon))
		item.emplace_back(&addLauncherIcon);
	item.emplace_back(&screenshot);
	item.emplace_back(&resetSessionOptions);
	item.emplace_back(&close);
}

}
