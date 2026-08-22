/*  This file is part of Saturn.emu.

	Saturn.emu is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Saturn.emu is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Saturn.emu.  If not, see <http://www.gnu.org/licenses/> */

module;
#include <mednafen/mednafen.h>
#include <mednafen/mempatcher-driver.h>

module system;

namespace EmuEx
{

using namespace Mednafen;

unsigned parseHex(const char* str) { return strtoul(str, nullptr, 16); }

static unsigned hexDigitCount(const char* str)
{
	unsigned count = 0;
	while(::isxdigit(static_cast<unsigned char>(*str))) { count++; str++; }
	return count;
}

static unsigned byteLengthFromHexDigits(unsigned digits)
{
	if(digits <= 2) return 1;
	if(digits <= 4) return 2;
	return 4;
}

void SaturnSystem::syncCheats()
{
	// Remove all existing mednafen patches
	int count = 0;
	MDFNI_ListCheats([](const Mednafen::MemoryPatch&, void* data) -> int
	{
		(*static_cast<int*>(data))++;
		return 1;
	}, &count);
	for(int i = count - 1; i >= 0; i--)
		MDFNI_DelCheat(i);
	// Re-add all enabled cheats
	for(auto& cheat: cheats)
	{
		if(!cheat.enabled) continue;
		for(auto& code: cheat.codes)
		{
			Mednafen::MemoryPatch patch;
			patch.name = cheat.name;
			patch.addr = code.addr;
			patch.val = code.val;
			patch.length = code.length;
			patch.bigendian = true;
			patch.status = true;
			if(code.compare >= 0)
			{
				patch.compare = code.compare;
				patch.type = 'C';
			}
			else
			{
				patch.type = 'R';
			}
			MDFNI_AddCheat(patch);
		}
	}
	// Persist cheat changes so they survive game/app restarts
	saveCheatsFile();
}

void SaturnSystem::saveCheatsFile()
{
	if(!hasContent())
	{
		log.warn("skipping cheat file save with no game loaded");
		return;
	}
	auto ctx = appContext();
	auto path = userFilePath(cheatsPath, ".cht");
	if(cheats.empty())
	{
		log.info("no cheats present, removing cheat file:{}", path);
		ctx.removeFileUri(path);
		return;
	}
	auto file = ctx.openFileUri(path, OpenFlags::testNewFile());
	if(!file)
	{
		log.warn("error creating cheat file:{}", path);
		return;
	}
	log.info("saving {} cheat(s) to:{}", cheats.size(), path);
	std::string buff;
	buff.reserve(cheats.size() * 64);
	for(auto& cheat: cheats)
	{
		// name may not contain '|' or newlines; trim at first separator
		auto name = std::string_view{cheat.name};
		auto pipePos = name.find('|');
		if(pipePos != std::string_view::npos)
			name = name.substr(0, pipePos);
		if(name.empty())
			name = "未命名";
		buff += std::format("cheat|{}|{}\n", name, cheat.enabled ? 1 : 0);
		for(auto& code: cheat.codes)
		{
			buff += std::format("code|{:X}|{:X}|{}|{}\n", code.addr, code.val, code.compare, code.length);
		}
	}
	file.write(buff.data(), buff.size());
}

void SaturnSystem::loadCheatsFile()
{
	// drop any cheats still in memory from a previous game
	cheats.clear();
	auto path = userFilePath(cheatsPath, ".cht");
	auto file = appContext().openFileUri(path, {.test = true, .accessHint = IOAccessHint::All});
	if(!file)
	{
		return;
	}
	log.info("reading cheat file:{}", path);
	char line[512];
	FileStream<FileIO> fileStream{std::move(file), "r"};
	Cheat* currentCheat = nullptr;
	while(fgets(line, sizeof(line), fileStream.filePtr()))
	{
		std::string_view lineStr{line};
		while(!lineStr.empty() && (lineStr.back() == '\n' || lineStr.back() == '\r'))
			lineStr.remove_suffix(1);
		if(lineStr.empty())
			continue;
		if(lineStr.starts_with("cheat|"))
		{
			auto rest = lineStr.substr(6);
			auto pipePos = rest.find('|');
			if(pipePos == std::string_view::npos)
				continue;
			auto name = rest.substr(0, pipePos);
			auto enabled = rest.substr(pipePos + 1);
			cheats.emplace_back(name);
			currentCheat = &cheats.back();
			currentCheat->enabled = enabled == "1";
		}
		else if(lineStr.starts_with("code|") && currentCheat)
		{
			unsigned addr{};
			unsigned long long val{};
			long long compare{-1};
			unsigned length{1};
			if(std::sscanf(lineStr.data(), "code|%x|%llx|%lld|%u", &addr, &val, &compare, &length) >= 2)
			{
				currentCheat->codes.emplace_back(uint32_t(addr), uint64_t(val), int64_t(compare), length ? length : 1);
			}
		}
	}
	log.info("loaded {} cheat(s) from file", cheats.size());
}

Cheat* SaturnSystem::newCheat(EmuApp& app, const char* name, CheatCodeDesc desc)
{
	auto* cPtr = &cheats.emplace_back(name[0] ? name : "未命名");
	if(!addCheatCode(app, cPtr, desc))
	{
		cheats.pop_back();
		return {};
	}
	log.info("added new cheat, {} total", cheats.size());
	return cPtr;
}

bool SaturnSystem::setCheatName(Cheat& c, const char* name)
{
	c.name = name;
	saveCheatsFile();
	return true;
}

std::string_view SaturnSystem::cheatName(const Cheat& c) const { return c.name; }

void SaturnSystem::setCheatEnabled(Cheat& c, bool on)
{
	c.enabled = on;
	syncCheats();
}

bool SaturnSystem::isCheatEnabled(const Cheat& c) const { return c.enabled; }

bool SaturnSystem::addCheatCode(EmuApp& app, Cheat*& cheatPtr, CheatCodeDesc desc)
{
	// Parse format: "address value [compare]" (space-separated, hex)
	const char* str = desc.str;
	if(!str || !*str)
	{
		app.postMessage(true, "无效输入");
		return false;
	}
	// Skip leading whitespace
	while(*str && ::isspace(static_cast<unsigned char>(*str))) str++;
	if(!*str)
	{
		app.postMessage(true, "无效输入");
		return false;
	}
	// Find first whitespace (separator between address and value)
	const char* p = str;
	while(*p && !::isspace(static_cast<unsigned char>(*p))) p++;
	if(!*p)
	{
		app.postMessage(true, "格式错误，请使用 地址 数值");
		return false;
	}
	std::string addrStr(str, p - str);
	auto addr = static_cast<uint32_t>(strtoul(addrStr.c_str(), nullptr, 16));
	// Skip whitespace
	while(*p && ::isspace(static_cast<unsigned char>(*p))) p++;
	if(!*p)
	{
		app.postMessage(true, "格式错误，请使用 地址 数值");
		return false;
	}
	// Find end of value (next whitespace or end)
	const char* valStart = p;
	while(*p && !::isspace(static_cast<unsigned char>(*p))) p++;
	std::string valStr(valStart, p - valStart);
	auto valDigits = hexDigitCount(valStr.c_str());
	auto val = strtoull(valStr.c_str(), nullptr, 16);
	auto length = byteLengthFromHexDigits(valDigits);
	// Optional compare value
	int64_t compare = -1;
	while(*p && ::isspace(static_cast<unsigned char>(*p))) p++;
	if(*p)
	{
		std::string compStr(p);
		compare = static_cast<int64_t>(strtoull(compStr.c_str(), nullptr, 16));
	}
	cheatPtr->codes.emplace_back(addr, val, compare, length);
	syncCheats();
	return true;
}

bool SaturnSystem::modifyCheatCode(EmuApp& app, Cheat& cheat, CheatCode& c, CheatCodeDesc desc)
{
	const char* str = desc.str;
	if(!str || !*str)
		return false;
	// Skip leading whitespace
	while(*str && ::isspace(static_cast<unsigned char>(*str))) str++;
	if(!*str) return false;
	// Find first whitespace
	const char* p = str;
	while(*p && !::isspace(static_cast<unsigned char>(*p))) p++;
	if(!*p)
	{
		app.postMessage(true, "格式错误，请使用 地址 数值");
		return false;
	}
	std::string addrStr(str, p - str);
	c.addr = static_cast<uint32_t>(strtoul(addrStr.c_str(), nullptr, 16));
	// Skip whitespace
	while(*p && ::isspace(static_cast<unsigned char>(*p))) p++;
	if(!*p)
	{
		app.postMessage(true, "格式错误，请使用 地址 数值");
		return false;
	}
	const char* valStart = p;
	while(*p && !::isspace(static_cast<unsigned char>(*p))) p++;
	std::string valStr(valStart, p - valStart);
	auto valDigits = hexDigitCount(valStr.c_str());
	c.val = strtoull(valStr.c_str(), nullptr, 16);
	c.length = byteLengthFromHexDigits(valDigits);
	// Optional compare value
	while(*p && ::isspace(static_cast<unsigned char>(*p))) p++;
	if(*p)
	{
		std::string compStr(p);
		c.compare = static_cast<int64_t>(strtoull(compStr.c_str(), nullptr, 16));
	}
	else
	{
		c.compare = -1;
	}
	syncCheats();
	return true;
}

Cheat* SaturnSystem::removeCheatCode(Cheat& c, CheatCode& code)
{
	c.codes.erase(toIterator(c.codes, code));
	bool removedAllCodes = c.codes.empty();
	if(removedAllCodes)
		cheats.erase(toIterator(cheats, c));
	syncCheats();
	return removedAllCodes ? nullptr : &c;
}

bool SaturnSystem::removeCheat(Cheat& c)
{
	cheats.erase(toIterator(cheats, c));
	syncCheats();
	return true;
}

void SaturnSystem::forEachCheat(DelegateFunc<bool(Cheat&, std::string_view)> del)
{
	for(auto& c: cheats)
	{
		if(!del(c, std::string_view{c.name}))
			break;
	}
}

void SaturnSystem::forEachCheatCode(Cheat& cheat, DelegateFunc<bool(CheatCode&, std::string_view)> del)
{
	for(auto& c: cheat.codes)
	{
		std::string code = std::format("{:X} {:X}", c.addr, c.val);
		if(c.compare >= 0)
			code += std::format(" {:X}", c.compare);
		del(c, std::string_view{code});
	}
}

// Batch add: parse grouped cheat text
// Format:
//   name|
//   address value
//   address value
//   name2|
//   address value
// Or without name: each "address value" line creates a new cheat
int SaturnSystem::batchAddCheats(EmuApp& app, const char* text)
{
	int count = 0;
	std::string_view input{text};
	Cheat* currentCheat = nullptr;
	auto lines = std::views::split(input, '\n');
	for(auto line: lines)
	{
		std::string_view lineStr{line};
		// Trim whitespace
		while(!lineStr.empty() && ::isspace(static_cast<unsigned char>(lineStr.front())))
			lineStr.remove_prefix(1);
		while(!lineStr.empty() && ::isspace(static_cast<unsigned char>(lineStr.back())))
			lineStr.remove_suffix(1);
		if(lineStr.empty()) continue;
		// Check if line is a cheat name (ends with |)
		if(lineStr.back() == '|')
		{
			std::string name{lineStr.substr(0, lineStr.size() - 1)};
			if(name.empty())
				name = "金手指 " + std::to_string(cheats.size() + 1);
			currentCheat = &cheats.emplace_back(name);
			count++;
			continue;
		}
		// Check if line has name|code format (inline)
		auto pipePos = lineStr.find('|');
		if(pipePos != std::string_view::npos)
		{
			std::string name{lineStr.substr(0, pipePos)};
			std::string code{lineStr.substr(pipePos + 1)};
			if(name.empty())
				name = "金手指 " + std::to_string(cheats.size() + 1);
			currentCheat = &cheats.emplace_back(name);
			count++;
			if(!code.empty())
			{
				auto* cPtr = currentCheat;
				addCheatCode(app, cPtr, {code.c_str(), 0});
			}
			continue;
		}
		// This line is a code for the current cheat
		if(!currentCheat)
		{
			// No cheat created yet, create a default one
			currentCheat = &cheats.emplace_back("金手指 " + std::to_string(cheats.size() + 1));
			count++;
		}
		auto* cPtr = currentCheat;
		addCheatCode(app, cPtr, {std::string{lineStr}.c_str(), 0});
	}
	// Enable all newly added cheats
	for(auto& c: cheats)
		c.enabled = true;
	syncCheats();
	return count;
}

}
