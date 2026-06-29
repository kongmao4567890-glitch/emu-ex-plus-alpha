/*  This file is part of Snes9x EX.

	Please see COPYING file in root directory for license information. */

#ifndef SNES9X_VERSION_1_4
#include <apu/apu.h>
#include <apu/bapu/snes/snes.hpp>
#include <ppu.h>
#endif
import system;
import emuex;
import imagine;
import std;

namespace EmuEx
{

using namespace IG;
using MainAppHelper = EmuAppHelperBase<MainApp>;

constexpr bool HAS_NSRT = !IS_SNES9X_VERSION_1_4;

#ifndef SNES9X_VERSION_1_4
class CustomAudioOptionView : public AudioOptionView, public MainAppHelper
{
	using MainAppHelper::system;

	void setDSPInterpolation(uint8_t val)
	{
		Snes9xSystem::log.info("set DSP interpolation:{}", val);
		system().optionAudioDSPInterpolation = val;
		SNES::dsp.spc_dsp.interpolation = val;
	}

	TextMenuItem dspInterpolationItem[5]
	{
		{"无",     attachParams(), [this](){ setDSPInterpolation(0); }},
		{"Linear",   attachParams(), [this](){ setDSPInterpolation(1); }},
		{"Gaussian", attachParams(), [this](){ setDSPInterpolation(2); }},
		{"Cubic",    attachParams(), [this](){ setDSPInterpolation(3); }},
		{"Sinc",     attachParams(), [this](){ setDSPInterpolation(4); }},
	};

	MultiChoiceMenuItem dspInterpolation
	{
		"DSP Interpolation", attachParams(),
		system().optionAudioDSPInterpolation.value(),
		dspInterpolationItem
	};

public:
	CustomAudioOptionView(ViewAttachParams attach, EmuAudio &audio): AudioOptionView{attach, audio, true}
	{
		loadStockItems();
		item.emplace_back(&dspInterpolation);
	}
};
#endif

class ConsoleOptionView : public TableView, public MainAppHelper
{
	BoolMenuItem multitap
	{
		"五人适配器", attachParams(),
		(bool)system().optionMultitap,
		[this](BoolMenuItem &item)
		{
			system().sessionOptionSet();
			system().optionMultitap = item.flipBoolValue(*this);
			system().setupSNESInput(app().defaultVController());
		}
	};

	TextMenuItem inputPortsItem[HAS_NSRT ? 5 : 4]
	{
		#ifndef SNES9X_VERSION_1_4
		{"Auto (NSRT)", attachParams(), setInputPortsDel(), {.id = SNES_AUTO_INPUT}},
		#endif
		{"Gamepads",    attachParams(), setInputPortsDel(), {.id = SNES_JOYPAD}},
		{"Superscope",  attachParams(), setInputPortsDel(), {.id = SNES_SUPERSCOPE}},
		{"Justifier",   attachParams(), setInputPortsDel(), {.id = SNES_JUSTIFIER}},
		{"Mouse",       attachParams(), setInputPortsDel(), {.id = SNES_MOUSE_SWAPPED}},
	};

	MultiChoiceMenuItem inputPorts
	{
		"输入端口", attachParams(),
		MenuId{system().snesInputPort},
		inputPortsItem
	};

	TextMenuItem::SelectDelegate setInputPortsDel()
	{
		return [this](TextMenuItem &item)
		{
			system().sessionOptionSet();
			system().optionInputPort = item.id;
			system().snesInputPort = item.id;
			system().setupSNESInput(app().defaultVController());
		};
	}

	TextMenuItem videoSystemItem[4]
	{
		{"自动",             attachParams(), [this](Input::Event e){ setVideoSystem(0, e); }},
		{"NTSC",             attachParams(), [this](Input::Event e){ setVideoSystem(1, e); }},
		{"PAL",              attachParams(), [this](Input::Event e){ setVideoSystem(2, e); }},
		{"NTSC + PAL Spoof", attachParams(), [this](Input::Event e){ setVideoSystem(3, e); }},
	};

	MultiChoiceMenuItem videoSystem
	{
		"系统", attachParams(),
		system().optionVideoSystem.value(),
		videoSystemItem
	};

	void setVideoSystem(int val, Input::Event e)
	{
		system().sessionOptionSet();
		system().optionVideoSystem = val;
		app().promptSystemReloadDueToSetOption(attachParams(), e);
	}

	TextHeadingMenuItem videoHeading{"视频", attachParams()};

	BoolMenuItem allowExtendedLines
	{
		"Allow Overscan Mode", attachParams(),
		(bool)system().optionAllowExtendedVideoLines,
		[this](BoolMenuItem &item)
		{
			system().sessionOptionSet();
			system().optionAllowExtendedVideoLines = item.flipBoolValue(*this);
		}
	};

	TextMenuItem deinterlaceModeItems[2]
	{
		{"Bob",   attachParams(), {.id = DeinterlaceMode::Bob}},
		{"Weave", attachParams(), {.id = DeinterlaceMode::Weave}},
	};

	MultiChoiceMenuItem deinterlaceMode
	{
		"Deinterlace Mode", attachParams(),
		MenuId{system().deinterlaceMode},
		deinterlaceModeItems,
		{
			.defaultItemOnSelect = [this](TextMenuItem &item)
			{
				system().sessionOptionSet();
				system().deinterlaceMode = DeinterlaceMode(item.id.val);
			}
		}
	};

	#ifndef SNES9X_VERSION_1_4
	TextHeadingMenuItem emulationHacks{"Emulation Hacks", attachParams()};

	BoolMenuItem blockInvalidVRAMAccess
	{
		"Allow Invalid VRAM Access", attachParams(),
		(bool)!system().optionBlockInvalidVRAMAccess,
		[this](BoolMenuItem &item)
		{
			system().sessionOptionSet();
			system().optionBlockInvalidVRAMAccess = !item.flipBoolValue(*this);
			PPU.BlockInvalidVRAMAccess = system().optionBlockInvalidVRAMAccess;
		}
	};

	BoolMenuItem separateEchoBuffer
	{
		"Separate Echo Buffer From Ram", attachParams(),
		(bool)system().optionSeparateEchoBuffer,
		[this](BoolMenuItem &item)
		{
			system().sessionOptionSet();
			system().optionSeparateEchoBuffer = item.flipBoolValue(*this);
			SNES::dsp.spc_dsp.separateEchoBuffer = system().optionSeparateEchoBuffer;
		}
	};

	void setSuperFXClock(unsigned val)
	{
		system().sessionOptionSet();
		system().optionSuperFXClockMultiplier = val;
		setSuperFXSpeedMultiplier(system().optionSuperFXClockMultiplier);
	}

	TextMenuItem superFXClockItem[2]
	{
		{"100%", attachParams(), [this]() { setSuperFXClock(100); }},
		{"自定义", attachParams(),
			[this](Input::Event e)
			{
				pushAndShowNewCollectValueInputView<int>(attachParams(), e, "Input 5 to 250", "",
					[this](CollectTextInputView&, auto val)
					{
						if(system().optionSuperFXClockMultiplier.isValid(val))
						{
							setSuperFXClock(val);
							superFXClock.setSelected(lastIndex(superFXClockItem), *this);
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
			}
		},
	};

	MultiChoiceMenuItem superFXClock
	{
		"SuperFX Clock Multiplier", attachParams(),
		[this]()
		{
			if(system().optionSuperFXClockMultiplier == 100)
				return 0;
			else
				return 1;
		}(),
		superFXClockItem,
		{
			.onSetDisplayString = [this](auto, Gfx::Text& t)
			{
				t.resetString(std::format("{}%", system().optionSuperFXClockMultiplier.value()));
				return true;
			}
		},
	};
	#endif

	std::array<MenuItem*, IS_SNES9X_VERSION_1_4 ? 6 : 10> menuItem
	{
		&inputPorts,
		&multitap,
		&videoHeading,
		&videoSystem,
		&allowExtendedLines,
		&deinterlaceMode,
		#ifndef SNES9X_VERSION_1_4
		&emulationHacks,
		&blockInvalidVRAMAccess,
		&separateEchoBuffer,
		&superFXClock,
		#endif
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

class CustomSystemActionsView : public SystemActionsView
{
private:
	TextMenuItem options
	{
		"主机选项", attachParams(),
		[this](TextMenuItem &, View &, Input::Event e)
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
		item.emplace_back(&options);
		loadStandardItems();
	}
};

class CustomFilePathOptionView : public FilePathOptionView, public MainAppHelper
{
	using MainAppHelper::system;
	using MainAppHelper::app;

	TextMenuItem cheatsPath
	{
		cheatsMenuName(appContext(), system().cheatsDir), attachParams(),
		[this](const Input::Event &e)
		{
			pushAndShow(makeViewWithName<UserPathSelectView>("金手指", system().userPath(system().cheatsDir),
				[this](CStringView path)
				{
					Snes9xSystem::log.info("set cheats path:{}", path);
					system().cheatsDir = path;
					cheatsPath.compile(cheatsMenuName(appContext(), path));
				}), e);
		}
	};

	TextMenuItem patchesPath
	{
		patchesMenuName(appContext(), system().patchesDir), attachParams(),
		[this](const Input::Event &e)
		{
			pushAndShow(makeViewWithName<UserPathSelectView>("Patches", system().userPath(system().patchesDir),
				[this](CStringView path)
				{
					Snes9xSystem::log.info("set patches path:{}", path);
					system().patchesDir = path;
					patchesPath.compile(patchesMenuName(appContext(), path));
				}), e);
		}
	};

	static std::string satMenuName(ApplicationContext ctx, std::string_view userPath)
	{
		return std::format("Satellaview Files: {}", userPathToDisplayName(ctx, userPath));
	}

	TextMenuItem satPath
	{
		satMenuName(appContext(), system().satDir), attachParams(),
		[this](const Input::Event &e)
		{
			pushAndShow(makeViewWithName<UserPathSelectView>("Satellaview Files", system().userPath(system().satDir),
				[this](CStringView path)
				{
					Snes9xSystem::log.info("set satellaview files path:{}", path);
					system().satDir = path;
					satPath.compile(satMenuName(appContext(), path));
				}), e);
		}
	};

	TextMenuItem bsxBios
	{
		bsxMenuName(system().bsxBiosPath), attachParams(),
		[this](const Input::Event &e)
		{
			pushAndShow(makeViewWithName<DataFileSelectView<>>("BS-X BIOS",
				app().validSearchPath(FS::dirnameUri(system().bsxBiosPath)),
				[this](CStringView path, FS::file_type)
				{
					system().bsxBiosPath = path;
					Snes9xSystem::log.info("set BS-X bios:{}", path);
					bsxBios.compile(bsxMenuName(path));
					return true;
				}, Snes9xSystem::hasBiosExtension), e);
		}
	};

	std::string bsxMenuName(CStringView path) const
	{
		return std::format("BS-X BIOS: {}", appContext().fileUriDisplayName(path));
	}

	TextMenuItem sufamiBios
	{
		sufamiMenuName(system().sufamiBiosPath), attachParams(),
		[this](const Input::Event &e)
		{
			pushAndShow(makeViewWithName<DataFileSelectView<>>("Sufami Turbo BIOS",
				app().validSearchPath(FS::dirnameUri(system().sufamiBiosPath)),
				[this](CStringView path, FS::file_type)
				{
					system().sufamiBiosPath = path;
					Snes9xSystem::log.info("set Sufami Turbo bios:{}", path);
					sufamiBios.compile(sufamiMenuName(path));
					return true;
				}, Snes9xSystem::hasBiosExtension), e);
		}
	};

	std::string sufamiMenuName(CStringView path) const
	{
		return std::format("Sufami Turbo BIOS: {}", appContext().fileUriDisplayName(path));
	}

public:
	CustomFilePathOptionView(ViewAttachParams attach): FilePathOptionView{attach, true}
	{
		loadStockItems();
		item.emplace_back(&cheatsPath);
		item.emplace_back(&patchesPath);
		item.emplace_back(&satPath);
		item.emplace_back(&bsxBios);
		item.emplace_back(&sufamiBios);
	}
};

class EditCheatView;

class EditRamCheatView: public TableView, public EmuAppHelper
{
public:
	EditRamCheatView(ViewAttachParams, CheatCode&, EditCheatView&);

private:
	CheatCode& code;
	EditCheatView& editCheatView;
	DualTextMenuItem addr, value, conditional;
	TextMenuItem remove;
};

class EditCheatView : public BaseEditCheatView
{
public:
	EditCheatView(ViewAttachParams attach, Cheat& cheat, BaseEditCheatsView& editCheatsView):
		BaseEditCheatView
		{
			"Edit Cheat",
			attach,
			cheat,
			editCheatsView
		}
		#ifndef SNES9X_VERSION_1_4
		,addCode
		{
			"Add Another Code", attach,
			[this](const Input::Event& e) { addNewCheatCode("Input xxxx-xxxx (GG), xxxxxxxx (AR), GF code, or blank", e); }
		}
		#endif
	{
		loadItems();
	}

	void loadItems()
	{
		codes.clear();
		system().forEachCheatCode(*cheatPtr, [this](CheatCode& c, std::string_view code)
		{
			codes.emplace_back("Code", code, attachParams(), [this, &c](const Input::Event& e)
			{
				pushAndShow(makeView<EditRamCheatView>(c, *this), e);
			});
			return true;
		});
		items.clear();
		items.emplace_back(&name);
		for(auto& c: codes)
		{
			items.emplace_back(&c);
		}
		#ifndef SNES9X_VERSION_1_4
		items.emplace_back(&addCode);
		#endif
		items.emplace_back(&remove);
	}

private:
	#ifndef SNES9X_VERSION_1_4
	TextMenuItem addCode;
	#endif
};

class EditCheatsView : public BaseEditCheatsView
{
public:
	EditCheatsView(ViewAttachParams attach, CheatsView& cheatsView):
		BaseEditCheatsView
		{
			attach,
			cheatsView,
			[this](ItemMessage msg) -> ItemReply
			{
				return msg.visit(overloaded
				{
					[&](const ItemsMessage&) -> ItemReply { return 1 + cheats.size(); },
					[&](const GetItemMessage& m) -> ItemReply
					{
						switch(m.idx)
						{
							case 0: return &addCode;
							default: return &cheats[m.idx - 1];
						}
					},
				});
			}
		},
		addCode
		{
			"Add Game Genie/Action Replay/Gold Finger Code", attach,
			[this](const Input::Event& e) { addNewCheat("Input xxxx-xxxx (GG), xxxxxxxx (AR), or GF code", e); }
		} {}

private:
	TextMenuItem addCode;
};

EditRamCheatView::EditRamCheatView(ViewAttachParams attach, CheatCode& code_, EditCheatView& editCheatView_):
	TableView
	{
		"Edit Memory Patch",
		attach,
		[this](ItemMessage msg) -> ItemReply
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply { return 4u; },
				[&](const GetItemMessage& m) -> ItemReply
				{
					switch(m.idx)
					{
						case 0: return &addr;
						case 1: return &value;
						case 2: return &conditional;
						case 3: return &remove;
						default: std::unreachable();
					}
				},
			});
		}
	},
	code{code_},
	editCheatView{editCheatView_},
	addr
	{
		"Address",
		std::format("{:x}", code_.address),
		attach,
		[this](const Input::Event& e)
		{
			pushAndShowNewCollectValueInputView<const char*>(attachParams(), e, "Input 6-digit hex", std::format("{:x}", code.address),
				[this](CollectTextInputView&, auto str)
				{
					unsigned a = parseHex(str);
					if(a > 0xFFFFFF)
					{
						app().postMessage(true, "value must be <= FFFFFF");
						return false;
					}
					setCheatAddress(code, a);
					addr.set2ndName(str);
					addr.place();
					editCheatView.loadItems();
					return true;
				});
		}
	},
	value
	{
		"Value",
		std::format("{:x}", code_.byte),
		attach,
		[this](const Input::Event& e)
		{
			pushAndShowNewCollectValueInputView<const char*>(attachParams(), e, "Input 2-digit hex", std::format("{:x}", code.byte),
				[this](CollectTextInputView&, auto str)
				{
					unsigned a = parseHex(str);
					if(a > 0xFF)
					{
						app().postMessage(true, "value must be <= FF");
						return false;
					}
					setCheatValue(code, a);
					value.set2ndName(str);
					value.place();
					editCheatView.loadItems();
					return true;
				});
		}
	},
	conditional
	{
		#ifndef SNES9X_VERSION_1_4
		"Conditional Value",
		#else
		"Saved Value",
		#endif
		codeConditionalToString(code_),
		attach,
		[this](const Input::Event& e)
		{
			pushAndShowNewCollectValueInputView<const char*, ScanValueMode::AllowBlank>(attachParams(), e, "Input 2-digit hex or blank", codeConditionalToString(code),
				[this](CollectTextInputView &, const char *str)
				{
					int a = -1;
					if(std::strlen(str))
					{
						a = parseHex(str);
						if(a > 0xFF)
						{
							app().postMessage(true, "value must be <= FF");
							return true;
						}
					}
					setCheatConditionalValue(code, a);
					conditional.set2ndName(str);
					conditional.place();
					editCheatView.loadItems();
					return true;
				});
		}
	},
	remove
	{
		"删除", attach,
		[this](const Input::Event& e)
		{
			pushAndShowModal(makeView<YesNoAlertView>("Really delete this patch?",
				YesNoAlertView::Delegates{.onYes = [this]{ editCheatView.removeCheatCode(code); dismiss(); }}), e);
		}
	} {}

std::unique_ptr<View> EmuApp::makeCustomView(ViewAttachParams attach, ViewID id)
{
	switch(id)
	{
		#ifndef SNES9X_VERSION_1_4
		case ViewID::AUDIO_OPTIONS: return std::make_unique<CustomAudioOptionView>(attach, audio);
		#endif
		case ViewID::FILE_PATH_OPTIONS: return std::make_unique<CustomFilePathOptionView>(attach);
		case ViewID::SYSTEM_ACTIONS: return std::make_unique<CustomSystemActionsView>(attach);
		default: return nullptr;
	}
}

std::unique_ptr<View> AppMeta::makeEditCheatsView(ViewAttachParams attach, CheatsView& view) { return std::make_unique<EditCheatsView>(attach, view); }
std::unique_ptr<View> AppMeta::makeEditCheatView(ViewAttachParams attach, Cheat& c, BaseEditCheatsView& baseView) { return std::make_unique<EditCheatView>(attach, c, baseView); }

}
