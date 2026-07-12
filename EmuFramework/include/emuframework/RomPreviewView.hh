#pragma once

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

#include <emuframework/defs.hh>
#ifndef IG_USE_MODULE_IMAGINE
#include <imagine/gui/TableView.hh>
#include <imagine/gui/MenuItem.hh>
#endif

namespace EmuEx
{

class RomPreviewView: public TableView, public EmuAppHelper
{
public:
	RomPreviewView(ViewAttachParams, const Input::Event &e);

	void place() override;
	void draw(Gfx::RendererCommands &__restrict__ cmds, ViewDrawParams) const override;
	void prepareDraw() override;

private:
	TextMenuItem playItem;
	TextMenuItem backItem;
	Input::Event launchEvent;

	void runPreviewFrames();
};

}
