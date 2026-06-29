/*  This file is part of 2600.emu.

	2600.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	2600.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with 2600.emu.  If not, see <http://www.gnu.org/licenses/> */

#include <stella/emucore/Props.hxx>
#include <stella/emucore/Control.hxx>
#include <stella/emucore/Paddles.hxx>
#include <SoundEmuEx.hh>
import system;
import emuex;
import imagine;

namespace EmuEx
{

using namespace IG;
using MainAppHelper = EmuAppHelperBase<MainApp>;

class CustomAudioOptionView : public AudioOptionView, public MainAppHelper
{
	using MainAppHelper::system;

	TextMenuItem::SelectDelegate setResampleQualityDel()
	{
		return [this](TextMenuItem &item)
		{
			A2600System::log.info("set resampling quality:{}", item.id.val);
			system().optionAudioResampleQuality = AudioSettings::ResamplingQuality(item.id.val);
			system().osystem.soundEmuEx().setResampleQuality(system().optionAudioResampleQuality);
		};
	}

	TextMenuItem resampleQualityItem[3]
	{
		{"低",   attachParams(), setResampleQualityDel(), {.id = AudioSettings::ResamplingQuality::nearestNeighbour}},
		{"高",  attachParams(), setResampleQualityDel(), {.id = AudioSettings::ResamplingQuality::lanczos_2}},
		{"超高", attachParams(), setResampleQualityDel(), {.id = AudioSettings::ResamplingQuality::lanczos_3}},
	};

	MultiChoiceMenuItem resampleQuality
	{
		"重采样质量", attachParams(),
		MenuId{system().optionAudioResampleQuality.value()},
		resampleQualityItem
	};

public:
	CustomAudioOptionView(ViewAttachParams attach, EmuAudio& audio): AudioOptionView{attach, audio, true}
	{
		loadStockItems();
		item.emplace_back(&resampleQuality);
	}
};

class CustomVideoOptionView : public VideoOptionView, public MainAppHelper
{
	using MainAppHelper::system;

	TextMenuItem tvPhosphorBlendItem[4]
	{
		{"70%",  attachParams(), setTVPhosphorBlendDel(), {.id = 70}},
		{"80%",  attachParams(), setTVPhosphorBlendDel(), {.id = 80}},
		{"90%",  attachParams(), setTVPhosphorBlendDel(), {.id = 90}},
		{"100%", attachParams(), setTVPhosphorBlendDel(), {.id = 100}},
	};

	MultiChoiceMenuItem tvPhosphorBlend
	{
		"电视荧光粉混合", attachParams(),
		MenuId{system().optionTVPhosphorBlend},
		tvPhosphorBlendItem
	};

	TextMenuItem::SelectDelegate setTVPhosphorBlendDel()
	{
		return [this](TextMenuItem &item)
		{
			system().optionTVPhosphorBlend = item.id;
			system().setRuntimeTVPhosphor(system().optionTVPhosphor, item.id);
		};
	}

public:
	CustomVideoOptionView(ViewAttachParams attach, EmuVideoLayer& layer): VideoOptionView{attach, layer, true}
	{
		loadStockItems();
		item.emplace_back(&systemSpecificHeading);
		item.emplace_back(&tvPhosphorBlend);
	}
};

class ConsoleOptionView : public TableView, public MainAppHelper
{
	TextMenuItem tvPhosphorItem[3]
	{
		{"关",  attachParams(), setTVPhosphorDel(), {.id = 0}},
		{"开",   attachParams(), setTVPhosphorDel(), {.id = 1}},
		{"自动", attachParams(), setTVPhosphorDel(), {.id = TV_PHOSPHOR_AUTO}},
	};

	MultiChoiceMenuItem tvPhosphor
	{
		"模拟电视荧光粉", attachParams(),
		MenuId{system().optionTVPhosphor},
		tvPhosphorItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(idx == 2 && system().osystem.hasConsole())
				{
					bool phospherInUse = system().osystem.console().properties().get(PropType::Display_Phosphor) == "YES";
					t.resetString(phospherInUse ? "开" : "关");
					return true;
				}
				else
					return false;
			}
		},
	};

	TextMenuItem videoSystemItem[7]
	{
		{"自动",     attachParams(), setVideoSystemDel(), {.id = 0}},
		{"NTSC",     attachParams(), setVideoSystemDel(), {.id = 1}},
		{"PAL",      attachParams(), setVideoSystemDel(), {.id = 2}},
		{"SECAM",    attachParams(), setVideoSystemDel(), {.id = 3}},
		{"NTSC 50",  attachParams(), setVideoSystemDel(), {.id = 4}},
		{"PAL 60",   attachParams(), setVideoSystemDel(), {.id = 5}},
		{"SECAM 60", attachParams(), setVideoSystemDel(), {.id = 6}},
	};

	MultiChoiceMenuItem videoSystem
	{
		"视频系统", attachParams(),
		MenuId{system().optionVideoSystem},
		videoSystemItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				if(idx == 0 && system().osystem.hasConsole())
				{
					t.resetString(system().osystem.console().about().DisplayFormat.c_str());
					return true;
				}
				else
					return false;
			}
		},
	};

	TextMenuItem::SelectDelegate setTVPhosphorDel()
	{
		return [this](TextMenuItem &item)
		{
			system().sessionOptionSet();
			system().optionTVPhosphor = item.id;
			system().setRuntimeTVPhosphor(item.id, system().optionTVPhosphorBlend);
		};
	}

	TextMenuItem::SelectDelegate setVideoSystemDel()
	{
		return [this](TextMenuItem &item, const Input::Event &e)
		{
			system().sessionOptionSet();
			system().optionVideoSystem = item.id;
			app().promptSystemReloadDueToSetOption(attachParams(), e);
		};
	}

	TextMenuItem inputPortsItem[5]
	{
		{"自动",            attachParams(), setInputPortsDel(), {.id = Controller::Type::Unknown}},
		{"摇杆",        attachParams(), setInputPortsDel(), {.id = Controller::Type::Joystick}},
		{"Paddles",         attachParams(), setInputPortsDel(), {.id = Controller::Type::Paddles}},
		{"Genesis 手柄", attachParams(), setInputPortsDel(), {.id = Controller::Type::Genesis}},
		{"Booster Grip",    attachParams(), setInputPortsDel(), {.id = Controller::Type::BoosterGrip}},
	};

	MultiChoiceMenuItem inputPorts
	{
		"输入端口", attachParams(),
		MenuId{system().optionInputPort1.value()},
		inputPortsItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text& t)
			{
				if(idx == 0 && system().osystem.hasConsole())
				{
					t.resetString(asString(system().osystem.console().leftController().type()));
					return true;
				}
				else
					return false;
			}
		},
	};

	TextMenuItem::SelectDelegate setInputPortsDel()
	{
		return [this](TextMenuItem &item)
		{
			system().sessionOptionSet();
			system().optionInputPort1 = Controller::Type(item.id.val);
			if(system().osystem.hasConsole())
			{
				system().setControllerType(app(), system().osystem.console(), Controller::Type(item.id.val));
			}
		};
	}

	TextMenuItem aPaddleRegionItem[4]
	{
		{"关",        attachParams(), setAPaddleRegionDel(), {.id = PaddleRegionMode::OFF}},
		{"左半部分",  attachParams(), setAPaddleRegionDel(), {.id = PaddleRegionMode::LEFT}},
		{"右半部分", attachParams(), setAPaddleRegionDel(), {.id = PaddleRegionMode::RIGHT}},
		{"完整",       attachParams(), setAPaddleRegionDel(), {.id = PaddleRegionMode::FULL}},
	};

	MultiChoiceMenuItem aPaddleRegion
	{
		"模拟拨盘区域", attachParams(),
		MenuId{system().optionPaddleAnalogRegion},
		aPaddleRegionItem
	};

	TextMenuItem::SelectDelegate setAPaddleRegionDel()
	{
		return [this](TextMenuItem &item)
		{
			system().sessionOptionSet();
			system().updatePaddlesRegionMode(app(), (PaddleRegionMode)item.id.val);
		};
	}

	TextMenuItem dPaddleSensitivityItem[2]
	{
		{"默认", attachParams(), [this]() { setDPaddleSensitivity(1); }, {.id = 1}},
		{"自定义", attachParams(),
			[this](const Input::Event &e)
			{
				pushAndShowNewCollectValueInputView<int>(attachParams(), e, "Input 1 to 20", "",
					[this](CollectTextInputView&, auto val)
					{
						if(system().optionPaddleDigitalSensitivity.isValid(val))
						{
							setDPaddleSensitivity(val);
							dPaddleSensitivity.setSelected(lastIndex(dPaddleSensitivityItem), *this);
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
		}
	};

	MultiChoiceMenuItem dPaddleSensitivity
	{
		"数字拨盘灵敏度", attachParams(),
		MenuId{system().optionPaddleDigitalSensitivity},
		dPaddleSensitivityItem,
		{
			.onSetDisplayString = [this](auto idx, Gfx::Text &t)
			{
				t.resetString(std::format("{}", system().optionPaddleDigitalSensitivity.value()));
				return true;
			}
		},
	};

	void setDPaddleSensitivity(uint8_t val)
	{
		system().sessionOptionSet();
		system().optionPaddleDigitalSensitivity = val;
		Paddles::setDigitalSensitivity(system().optionPaddleDigitalSensitivity);
	}

	std::array<MenuItem*, 5> menuItem
	{
		&tvPhosphor,
		&videoSystem,
		&inputPorts,
		&aPaddleRegion,
		&dPaddleSensitivity,
	};

public:
	ConsoleOptionView(ViewAttachParams attach):
		TableView
		{
			"主机选项",
			attach,
			menuItem
		}
	{}
};

class VCSSwitchesView : public TableView, public MainAppHelper
{
	BoolMenuItem diff1
	{
		"左 (P1) 难度", attachParams(),
		system().p1DiffB,
		"A", "B",
		[this](BoolMenuItem& item)
		{
			system().p1DiffB = item.flipBoolValue(*this);
		}
	};

	BoolMenuItem diff2
	{
		"右 (P2) 难度", attachParams(),
		system().p2DiffB,
		"A", "B",
		[this](BoolMenuItem& item)
		{
			system().p2DiffB = item.flipBoolValue(*this);
		}
	};

	BoolMenuItem color
	{
		"颜色", attachParams(),
		system().vcsColor,
		[this](BoolMenuItem& item)
		{
			system().vcsColor = item.flipBoolValue(*this);
		}
	};

	std::array<MenuItem*, 3> menuItem
	{
		&diff1,
		&diff2,
		&color
	};

public:
	VCSSwitchesView(ViewAttachParams attach):
		TableView
		{
			"开关",
			attach,
			menuItem
		}
	{}

	void onShow() final
	{
		diff1.setBoolValue(system().p1DiffB, *this);
		diff2.setBoolValue(system().p2DiffB, *this);
		color.setBoolValue(system().vcsColor, *this);
	}

};

class CustomSystemActionsView : public SystemActionsView
{
private:
	TextMenuItem switches
	{
		"主机开关", attachParams(),
		[this](const Input::Event &e)
		{
			if(system().hasContent())
			{
				pushAndShow(makeView<VCSSwitchesView>(), e);
			}
		}
	};

	TextMenuItem options
	{
		"主机选项", attachParams(),
		[this](const Input::Event &e)
		{
			if(system().hasContent())
			{
				pushAndShow(makeView<ConsoleOptionView>(), e);
			}
		}
	};

public:
	CustomSystemActionsView(ViewAttachParams attach): SystemActionsView{attach, true}
	{
		item.emplace_back(&switches);
		item.emplace_back(&options);
		loadStandardItems();
	}
};

std::unique_ptr<View> EmuApp::makeCustomView(ViewAttachParams attach, ViewID id)
{
	switch(id)
	{
		case ViewID::AUDIO_OPTIONS: return std::make_unique<CustomAudioOptionView>(attach, audio);
		case ViewID::VIDEO_OPTIONS: return std::make_unique<CustomVideoOptionView>(attach, videoLayer);
		case ViewID::SYSTEM_ACTIONS: return std::make_unique<CustomSystemActionsView>(attach);
		default: return nullptr;
	}
}

}
