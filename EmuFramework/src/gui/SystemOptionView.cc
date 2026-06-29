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

#include <emuframework/SystemOptionView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/viewUtils.hh>
#include "CPUAffinityView.hh"
import imagine;

namespace EmuEx
{

SystemOptionView::SystemOptionView(ViewAttachParams attach, bool customMenu):
	TableView{"系统选项", attach, item},
	autosaveTimerItem
	{
		{"关",    attach, {.id = 0}},
		{"5min",  attach, {.id = 5}},
		{"10min", attach, {.id = 10}},
		{"15min", attach, {.id = 15}},
		{"自定义", attach, [this](const Input::Event &e)
			{
				pushAndShowNewCollectValueRangeInputView<int, 0, maxAutosaveSaveFreq.count()>(attachParams(), e, "Input 0 to 720", "",
					[this](CollectTextInputView &, auto val)
					{
						app().autosaveManager.saveTimer.frequency = Minutes{val};
						autosaveTimer.setSelected(MenuId{val}, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	autosaveTimer
	{
		"计时器", attach,
		MenuId{app().autosaveManager.saveTimer.frequency.count()},
		autosaveTimerItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(!idx)
					return false;
				t.resetString(std::format("{}", app().autosaveManager.saveTimer.frequency));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().autosaveManager.saveTimer.frequency = Minutes{item.id}; }
		},
	},
	autosaveLaunchItem
	{
		{"主槽",            attach, {.id = AutosaveLaunchMode::Load}},
		{"主槽（无状态）", attach, {.id = AutosaveLaunchMode::LoadNoState}},
		{"无存档槽",         attach, {.id = AutosaveLaunchMode::NoSave}},
		{"选择槽位",          attach, {.id = AutosaveLaunchMode::Ask}},
	},
	autosaveLaunch
	{
		"启动模式", attach,
		MenuId{app().autosaveManager.autosaveLaunchMode},
		autosaveLaunchItem,
		{
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().autosaveManager.autosaveLaunchMode = AutosaveLaunchMode(item.id.val); }
		},
	},
	autosaveContent
	{
		"内容", attach,
		app().autosaveManager.saveOnlyBackupMemory,
		"状态和备份内存", "仅备份内存",
		[this](BoolMenuItem &item)
		{
			app().autosaveManager.saveOnlyBackupMemory = item.flipBoolValue(*this);
		}
	},
	confirmOverwriteState
	{
		"覆盖存档时确认", attach,
		app().confirmOverwriteState,
		[this](BoolMenuItem &item)
		{
			app().confirmOverwriteState = item.flipBoolValue(*this);
		}
	},
	fastModeSpeedItem
	{
		{"1.5x",  attach, {.id = 150}},
		{"2x",    attach, {.id = 200}},
		{"4x",    attach, {.id = 400}},
		{"8x",    attach, {.id = 800}},
		{"16x",   attach, {.id = 1600}},
		{"自定义", attach,
			[this](const Input::Event &e)
			{
				pushAndShowNewCollectValueRangeInputView<float, 1, 20>(attachParams(), e, "Input above 1.0 to 20.0", "",
					[this](CollectTextInputView &, auto val)
					{
						auto valAsInt = std::round(val * 100.f);
						app().setAltSpeed(AltSpeedMode::fast, valAsInt);
						fastModeSpeed.setSelected(MenuId{valAsInt}, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	fastModeSpeed
	{
		"快进速度", attach,
		MenuId{app().altSpeed(AltSpeedMode::fast)},
		fastModeSpeedItem,
		{
			.onSetDisplayString = [this](auto, Gfx::Text& t)
			{
				t.resetString(std::format("{:g}x", app().altSpeedAsDouble(AltSpeedMode::fast)));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setAltSpeed(AltSpeedMode::fast, item.id); }
		},
	},
	slowModeSpeedItem
	{
		{"0.25x", attach, {.id = 25}},
		{"0.50x", attach, {.id = 50}},
		{"自定义", attach,
			[this](const Input::Event &e)
			{
				pushAndShowNewCollectValueInputView<float>(attachParams(), e, "Input 0.05 up to 1.0", "",
					[this](CollectTextInputView &, auto val)
					{
						auto valAsInt = std::round(val * 100.f);
						if(app().setAltSpeed(AltSpeedMode::slow, valAsInt))
						{
							slowModeSpeed.setSelected(MenuId{valAsInt}, *this);
							dismissPrevious();
							return true;
						}
						else
						{
							app().postErrorMessage("值超出范围");
							return false;
						}
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	slowModeSpeed
	{
		"慢放速度", attach,
		MenuId{app().altSpeed(AltSpeedMode::slow)},
		slowModeSpeedItem,
		{
			.onSetDisplayString = [this](auto, Gfx::Text& t)
			{
				t.resetString(std::format("{:g}x", app().altSpeedAsDouble(AltSpeedMode::slow)));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item) { app().setAltSpeed(AltSpeedMode::slow, item.id); }
		},
	},
	rewindStatesItem
	{
		{"0",  attach, {.id = 0}},
		{"30", attach, {.id = 30}},
		{"60", attach, {.id = 60}},
		{"自定义", attach, [this](const Input::Event &e)
			{
				pushAndShowNewCollectValueRangeInputView<int, 0, 50000>(attachParams(), e,
					"Input 0 to 50000", std::to_string(app().rewindManager.maxStates),
					[this](CollectTextInputView &, auto val)
					{
						app().rewindManager.updateMaxStates(val);
						rewindStates.setSelected(val, *this);
						dismissPrevious();
						return true;
					});
				return false;
			}, {.id = defaultMenuId}
		},
	},
	rewindStates
	{
		"状态数", attach,
		MenuId{app().rewindManager.maxStates},
		rewindStatesItem,
		{
			.onSetDisplayString = [this](auto, Gfx::Text& t)
			{
				t.resetString(std::format("{}", app().rewindManager.maxStates));
				return true;
			},
			.defaultItemOnSelect = [this](TextMenuItem &item)
			{
				app().rewindManager.updateMaxStates(item.id);
				app().defaultVController().updateEnabledUIButtons();
			}
		},
	},
	rewindTimeInterval
	{
		"状态间隔（秒）", std::to_string(app().rewindManager.saveTimer.frequency.count()), attach,
		[this](const Input::Event &e)
		{
			pushAndShowNewCollectValueRangeInputView<int, 1, 60>(attachParams(), e,
				"Input 1 to 60", std::to_string(app().rewindManager.saveTimer.frequency.count()),
				[this](CollectTextInputView &, auto val)
				{
					app().rewindManager.saveTimer.frequency = Seconds{val};
					rewindTimeInterval.set2ndName(std::to_string(val));
					return true;
				});
		}
	},
	performanceMode
	{
		"性能模式", attach,
		app().useSustainedPerformanceMode,
		"普通", "持续",
		[this](BoolMenuItem &item)
		{
			app().useSustainedPerformanceMode = item.flipBoolValue(*this);
		}
	},
	noopThread
	{
		"空操作线程（实验性）", attach,
		(bool)app().useNoopThread,
		[this](BoolMenuItem &item)
		{
			app().useNoopThread = item.flipBoolValue(*this);
		}
	},
	cpuAffinity
	{
		"配置 CPU 亲和性", attach,
		[this](const Input::Event &e)
		{
			pushAndShow(makeView<CPUAffinityView>(appContext().cpuCount()), e);
		}
	},
	autosaveHeading{"自动存档选项", attach},
	rewindHeading{"倒回选项", attach},
	otherHeading{"其他选项", attach}
{
	if(!customMenu)
	{
		loadStockItems();
	}
}

void SystemOptionView::loadStockItems()
{
	item.emplace_back(&autosaveHeading);
	item.emplace_back(&autosaveLaunch);
	item.emplace_back(&autosaveTimer);
	item.emplace_back(&autosaveContent);
	item.emplace_back(&rewindHeading);
	item.emplace_back(&rewindStates);
	item.emplace_back(&rewindTimeInterval);
	item.emplace_back(&otherHeading);
	item.emplace_back(&confirmOverwriteState);
	item.emplace_back(&fastModeSpeed);
	item.emplace_back(&slowModeSpeed);
	if(used(performanceMode) && appContext().hasSustainedPerformanceMode())
		item.emplace_back(&performanceMode);
	if(used(noopThread))
		item.emplace_back(&noopThread);
	if(used(cpuAffinity) && appContext().cpuCount() > 1)
		item.emplace_back(&cpuAffinity);
}

}
