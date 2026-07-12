/*  This file is part of EmuFramework.

	EmuFramework is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	EmuFramework is distributed in the hope that it will be useful,
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
#ifdef IG_USE_MODULES
import emuex;
import imagine;
import std;
#else
#include <imagine/gui/AlertView.hh>
#endif

namespace EmuEx
{

constexpr SystemLogger log{"RomPreviewView"};

RomPreviewView::RomPreviewView(ViewAttachParams attach, const Input::Event &e):
	TableView
	{
		"ROM预览",
		attach,
		[this](ItemMessage msg) -> ItemReply
		{
			return msg.visit(overloaded
			{
				[&](const ItemsMessage&) -> ItemReply { return 2u; },
				[&](const GetItemMessage& m) -> ItemReply
				{
					switch(m.idx)
					{
						case 0: return &playItem;
						case 1: return &backItem;
						default: std::unreachable();
					}
				},
			});
		}
	},
	playItem
	{
		"开始游戏", attachParams(),
		[this](const Input::Event& e)
		{
			auto &app = this->app();
			app.launchSystem(e);
		}
	},
	backItem
	{
		"返回", attachParams(),
		[this](const Input::Event& e)
		{
			auto &app = this->app();
			app.closeSystem();
			dismiss();
		}
	},
	launchEvent{e}
{
	runPreviewFrames();
}

void RomPreviewView::runPreviewFrames()
{
	auto &app = this->app();
	auto &sys = system();
	if(!sys.hasContent())
		return;
	// Set up video format for the loaded ROM
	app.setRenderPixelFormat(app.windowPixelFormat());
	// Run frames to get past boot screen (about 2 seconds of NES)
	constexpr int previewFrames = 120;
	for(auto i: iotaCount(previewFrames))
	{
		sys.runFrame({}, &app.video, nullptr);
	}
}

void RomPreviewView::prepareDraw()
{
	TableView::prepareDraw();
}

void RomPreviewView::place()
{
	TableView::place();
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent())
	{
		app.videoLayer.place({}, displayRect(), nullptr, sys);
	}
}

void RomPreviewView::draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams params) const
{
	auto &app = this->app();
	auto &sys = system();
	if(sys.hasContent())
	{
		app.videoLayer.draw(cmds);
	}
	// Draw semi-transparent background for the button area
	using namespace Gfx;
	auto &basicEffect = cmds.basicEffect();
	cmds.set(BlendMode::OFF);
	basicEffect.disableTexture(cmds);
	cmds.setColor({.0, .0, .0, .7});
	auto tableArea = params.viewRect;
	cmds.drawQuad(tableArea.as<int16_t>());
	// Draw table items on top
	TableView::draw(cmds, params);
}

}
