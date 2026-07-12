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

#include <emuframework/RomPreviewView.hh>
#include <emuframework/EmuApp.hh>
#include <emuframework/EmuSystem.hh>
#include <emuframework/EmuVideo.hh>
#include <emuframework/EmuVideoLayer.hh>
#include <emuframework/FilePicker.hh>
import imagine;

namespace EmuEx
{

using namespace IG;

[[maybe_unused]] constexpr SystemLogger log{"RomPreviewView"};

static constexpr int previewSpeed = 3;

RomPreviewView::RomPreviewView(ViewAttachParams attach, [[maybe_unused]] const Input::Event &e):
	TableView
	{
		"游戏列表",
		attach,
		[this](ItemMessage msg) -> ItemReply
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply { return 2u + romItems.size(); },
				[&](const GetItemMessage& m) -> ItemReply
				{
					auto romCount = romItems.size();
					if(m.idx == 0)
						return &playItem;
					if(m.idx == 1 + romCount)
						return &backItem;
					return &romItems[m.idx - 1];
				},
			});
		}
	},
	playItem
	{
		"\xe5\xbc\x80\xe5\xa7\x8b\xe6\xb8\xb8\xe6\x88\x8f", attach,
		[this](const Input::Event &e)
		{
			if(!hasContent)
				return;
			stopPreview();
			app().launchSystem(e);
		}
	},
	backItem
	{
		"\xe8\xbf\x94\xe5\x9b\x9e", attach,
		[this](const Input::Event&)
		{
			stopPreview();
			if(hasContent)
				app().closeSystem();
			dismiss();
		}
	},
	bgQuads{attach.rendererTask, {.size = 2}}
{
	scanDirectory(attach);
}

RomPreviewView::~RomPreviewView()
{
	stopPreview();
}

void RomPreviewView::onAddedToController(ViewController *vc, const Input::Event &e)
{
	TableView::onAddedToController(vc, e);
	if(system().hasContent())
	{
		hasContent = true;
		startPreview();
	}
}

void RomPreviewView::startPreview()
{
	if(isRunning)
		return;
	auto &app = this->app();
	auto &sys = system();
	if(!sys.hasContent())
		return;
	log.info("starting ROM preview at {}x speed", previewSpeed);
	for([[maybe_unused]] auto i: iotaCount(30))
	{
		sys.runFrame({}, &app.video, nullptr);
	}
	isRunning = true;
	onFrameDel =
		[this](FrameParams)
		{
			auto &sys = system();
			auto &app = this->app();
			if(!sys.hasContent())
				return false;
			for([[maybe_unused]] auto i: iotaCount(previewSpeed))
			{
				sys.runFrame({}, &app.video, nullptr);
			}
			return true;
		};
	app.emuWindow().addOnFrame(onFrameDel);
}

void RomPreviewView::stopPreview()
{
	if(!isRunning)
		return;
	isRunning = false;
	if(onFrameDel)
	{
		auto &app = this->app();
		app.emuWindow().removeOnFrame(onFrameDel);
		onFrameDel = {};
	}
}

void RomPreviewView::scanDirectory(ViewAttachParams attach)
{
	auto &app = this->app();
	auto &dir = app.contentSearchPath;
	if(dir.empty())
	{
		log.info("contentSearchPath is empty, no games to scan");
		return;
	}

	log.info("scanning directory: {}", dir);

	struct RomEntry
	{
		std::string path;
		std::string name;
	};

	std::vector<RomEntry> roms;
	try
	{
		appContext().forEachInDirectoryUri(dir,
			[&roms](auto &entry)
			{
				if(entry.type() == FS::file_type::directory)
					return true;
				if(!AppMeta::defaultFsFilter(entry.name()))
					return true;
				roms.push_back({std::string{entry.path()}, std::string{entry.name()}});
				return true;
			});
	}
	catch(...)
	{
		log.error("error scanning directory");
		return;
	}

	std::ranges::sort(roms,
		[](const RomEntry &a, const RomEntry &b)
		{
			return caselessLexCompare(a.name, b.name);
		});

	romItems.reserve(roms.size());
	romPaths.reserve(roms.size());
	romNames.reserve(roms.size());
	for(auto &rom : roms)
	{
		romPaths.push_back(rom.path);
		romNames.push_back(rom.name);
		auto idx = romPaths.size() - 1;
		romItems.emplace_back(
			rom.name, attach,
			[this, idx]([[maybe_unused]] const Input::Event &e)
			{
				loadRom(idx, e);
			}
		);
	}
	log.info("found {} ROMs in {}", roms.size(), dir);
}

void RomPreviewView::loadRom(size_t idx, [[maybe_unused]] const Input::Event &e)
{
	// 只注册帧回调，不在输入事件回调中做任何视图栈操作
	// lambda只捕获this+idx（16字节，恰好等于DelegateFunc存储大小）
	// 不能捕获string（32字节*2=64字节，远超16字节DelegateFunc限制）
	app().emuWindow().addOnFrame(
		[this, idx](FrameParams) -> bool
		{
			auto &app = this->app();
			auto path = romPaths[idx];
			auto name = romNames[idx];
			app.createSystemWithMedia(IO{}, path, name, appContext().defaultInputEvent(), {}, app.attachParams(),
				[this](const Input::Event &)
				{
					auto &app = this->app();
					app.recentContent.add(app.system());
					app.viewController().popToRoot();
					app.viewController().pushAndShow(std::make_unique<RomPreviewView>(app.attachParams(), appContext().defaultInputEvent()), appContext().defaultInputEvent());
				});
			return false; // 一次性回调
		});
}

void RomPreviewView::place()
{
	auto fullRect = viewRect();
	auto previewHeight = fullRect.ySize() * 40 / 100;
	auto previewRect = WindowRect{{fullRect.x, fullRect.y}, {fullRect.x2, fullRect.y + previewHeight}};
	auto bottomRect = WindowRect{{fullRect.x, fullRect.y + previewHeight}, {fullRect.x2, fullRect.y2}};

	setViewRect(bottomRect, bottomRect);
	TableView::place();

	auto &app = this->app();
	auto &sys = system();
	if(hasContent && sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.place({}, previewRect, nullptr, sys);
	}

	bgQuads.write(0, {.bounds = previewRect.as<int16_t>()});
	bgQuads.write(1, {.bounds = bottomRect.as<int16_t>()});
}

void RomPreviewView::draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams) const
{
	auto &app = this->app();
	auto &sys = system();
	using namespace Gfx;
	auto &basicEffect = cmds.basicEffect();

	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, 1.});
	cmds.drawQuad(bgQuads, 0);

	if(hasContent && sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.draw(cmds);
	}

	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, .7});
	cmds.drawQuad(bgQuads, 1);

	TableView::draw(cmds, {});
}

}
