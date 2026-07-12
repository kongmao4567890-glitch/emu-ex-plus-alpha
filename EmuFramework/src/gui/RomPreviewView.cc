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
import imagine;

namespace EmuEx
{

using namespace IG;

[[maybe_unused]] constexpr SystemLogger log{"RomPreviewView"};

// Run 3 emulated frames per screen frame (~3x speed at 60fps)
static constexpr int previewSpeed = 3;

RomPreviewView::RomPreviewView(ViewAttachParams attach, [[maybe_unused]] const Input::Event &e):
	TableView
	{
		"ROM Preview",
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
	startPreview();
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
	// Follow benchmark pattern: run frames directly without start()
	// Run a few frames first to initialize the video buffer
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
	auto &sys = system();
	auto &dir = sys.contentDirectory();
	if(dir.empty())
		return;

	struct RomEntry
	{
		FS::PathString path;
		std::string name;
	};

	std::vector<RomEntry> roms;
	try
	{
		appContext().forEachInDirectoryUri(dir,
			[&roms, &sys](auto &entry)
			{
				if(entry.type() == FS::file_type::directory)
					return true;
				if(!AppMeta::defaultFsFilter(entry.name()))
					return true;
				// Skip the currently loaded ROM
				if(entry.path() == sys.contentLocation())
					return true;
				roms.push_back({FS::PathString{entry.path()}, std::string{entry.name()}});
				return true;
			});
	}
	catch(...)
	{
		log.error("error scanning directory");
		return;
	}

	// Sort alphabetically (case-insensitive)
	std::ranges::sort(roms,
		[](const RomEntry &a, const RomEntry &b)
		{
			return caselessLexCompare(a.name, b.name);
		});

	// Create menu items for each ROM
	romItems.reserve(roms.size());
	romPaths.reserve(roms.size());
	romNames.reserve(roms.size());
	for(auto &rom : roms)
	{
		romPaths.push_back(std::string{rom.path});
		romNames.push_back(rom.name);
		auto idx = romPaths.size() - 1;
		romItems.emplace_back(
			rom.name, attach,
			[this, idx](const Input::Event &e)
			{
				stopPreview();
				auto &appRef = app();
				auto pathCopy = romPaths[idx];
				auto nameCopy = romNames[idx];
				dismiss();
				appRef.onSelectFileFromPicker(IO{}, pathCopy, nameCopy, e, {}, appRef.attachParams());
			}
		);
	}
}

void RomPreviewView::place()
{
	// Split layout: top 40% for preview, bottom 60% for game list
	auto fullRect = viewRect();
	auto previewHeight = fullRect.ySize() * 40 / 100;
	auto previewRect = WindowRect{{fullRect.x, fullRect.y}, {fullRect.x2, fullRect.y + previewHeight}};
	auto bottomRect = WindowRect{{fullRect.x, fullRect.y + previewHeight}, {fullRect.x2, fullRect.y2}};

	// Constrain table to bottom area
	setViewRect(bottomRect, bottomRect);
	TableView::place();

	// Place video in top area
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.place({}, previewRect, nullptr, sys);
	}

	// Background quads: index 0 = top (solid black), index 1 = bottom (semi-transparent)
	bgQuads.write(0, {.bounds = previewRect.as<int16_t>()});
	bgQuads.write(1, {.bounds = bottomRect.as<int16_t>()});
}

void RomPreviewView::draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams) const
{
	auto &app = this->app();
	auto &sys = system();
	using namespace Gfx;
	auto &basicEffect = cmds.basicEffect();

	// Top area: solid black background (behind video)
	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, 1.});
	cmds.drawQuad(bgQuads, 0);

	// Draw video on top of black background
	if(sys.hasContent() && app.video.hasRendererTask())
	{
		app.videoLayer.draw(cmds);
	}

	// Bottom area: semi-transparent black background
	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, .7});
	cmds.drawQuad(bgQuads, 1);

	// Draw table in bottom area
	TableView::draw(cmds, {});
}

}
