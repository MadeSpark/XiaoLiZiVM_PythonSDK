#define XLZSDK_EXPORTS

#include "xlz_appinfo.h"
#include "xlz_api_wrappers.h"
#include "xlz_encoding.h"
#include "xlz_event_utf8.h"
#include "xlz_events.h"
#include "xlz_exports.h"
#include "xlz_sdk.h"
#include "xlz_zh_names_utf8.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <random>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

static xlz::Sdk g_sdk;
static std::string g_appInfoJsonAcp;

enum class MatchType
{
	Exact = 0,
	Contains = 1,
	Fuzzy = 2,
	Regex = 3,
};

struct Rule
{
	bool enabled = true;
	MatchType type = MatchType::Contains;
	std::wstring pattern;
	std::vector<std::wstring> replies;
	int fuzzyThreshold = 75;
};

struct Config
{
	bool enabled = true;
	std::vector<Rule> rules;
};

static std::mutex g_configMutex;
static Config g_config;
static std::wstring g_configPath;
static std::atomic<HWND> g_settingsHwnd{ nullptr };

static std::string Utf8ToAcpLossy(const std::string& utf8)
{
	const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
	if (wlen <= 0)
	{
		return std::string();
	}
	std::wstring ws;
	ws.resize(static_cast<size_t>(wlen));
	::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), ws.data(), wlen);

	const int blen = ::WideCharToMultiByte(CP_ACP, 0, ws.data(), wlen, nullptr, 0, nullptr, nullptr);
	if (blen <= 0)
	{
		return std::string();
	}
	std::string out;
	out.resize(static_cast<size_t>(blen));
	::WideCharToMultiByte(CP_ACP, 0, ws.data(), wlen, out.data(), blen, nullptr, nullptr);
	return out;
}

static std::wstring Utf8ToWide(const std::string& s)
{
	if (s.empty())
	{
		return std::wstring();
	}
	const int wlen = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	if (wlen <= 0)
	{
		return std::wstring();
	}
	std::wstring ws;
	ws.resize(static_cast<size_t>(wlen));
	::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), wlen);
	return ws;
}

static std::string WideToUtf8(const std::wstring& ws)
{
	if (ws.empty())
	{
		return std::string();
	}
	const int len = ::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
	if (len <= 0)
	{
		return std::string();
	}
	std::string out;
	out.resize(static_cast<size_t>(len));
	::WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), out.data(), len, nullptr, nullptr);
	return out;
}

static bool FileExistsW(const std::wstring& path)
{
	const DWORD attrs = ::GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool DirectoryExistsW(const std::wstring& path)
{
	const DWORD attrs = ::GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static void EnsureDirectoryRecursiveW(std::wstring path)
{
	if (path.empty())
	{
		return;
	}
	std::replace(path.begin(), path.end(), L'/', L'\\');
	while (!path.empty() && (path.back() == L'\\' || path.back() == L' '))
	{
		path.pop_back();
	}
	if (path.size() >= 2 && path[1] == L':')
	{
	}
	size_t pos = 0;
	while (true)
	{
		pos = path.find(L'\\', pos);
		const std::wstring cur = (pos == std::wstring::npos) ? path : path.substr(0, pos);
		if (!cur.empty() && cur.back() != L':' && !DirectoryExistsW(cur))
		{
			::CreateDirectoryW(cur.c_str(), nullptr);
		}
		if (pos == std::wstring::npos)
		{
			break;
		}
		++pos;
	}
}

static void EnsureUnicodeIniFileW(const std::wstring& filePath)
{
	if (FileExistsW(filePath))
	{
		return;
	}
	const HANDLE h = ::CreateFileW(filePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
	{
		return;
	}
	const uint8_t bom[2] = { 0xFF, 0xFE };
	DWORD written = 0;
	::WriteFile(h, bom, 2, &written, nullptr);
	::CloseHandle(h);
}

static std::wstring ReadIniStringW(const std::wstring& iniPath, const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue)
{
	std::wstring buf;
	buf.resize(4096);
	const DWORD n = ::GetPrivateProfileStringW(section.c_str(), key.c_str(), defaultValue.c_str(), buf.data(), static_cast<DWORD>(buf.size()), iniPath.c_str());
	buf.resize(static_cast<size_t>(n));
	return buf;
}

static std::wstring MatchTypeToString(MatchType t)
{
	switch (t)
	{
	case MatchType::Exact: return L"exact";
	case MatchType::Contains: return L"contains";
	case MatchType::Fuzzy: return L"fuzzy";
	case MatchType::Regex: return L"regex";
	}
	return L"contains";
}

static MatchType MatchTypeFromString(std::wstring s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c)
	{
		if (c >= L'A' && c <= L'Z')
		{
			return static_cast<wchar_t>(c - L'A' + L'a');
		}
		return c;
	});
	if (s == L"exact")
	{
		return MatchType::Exact;
	}
	if (s == L"contains")
	{
		return MatchType::Contains;
	}
	if (s == L"fuzzy")
	{
		return MatchType::Fuzzy;
	}
	if (s == L"regex")
	{
		return MatchType::Regex;
	}
	return MatchType::Contains;
}

static std::wstring Trim(std::wstring s)
{
	auto isWs = [](wchar_t c) { return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n'; };
	while (!s.empty() && isWs(s.front()))
	{
		s.erase(s.begin());
	}
	while (!s.empty() && isWs(s.back()))
	{
		s.pop_back();
	}
	return s;
}

static std::vector<std::wstring> Split(const std::wstring& s, wchar_t delim)
{
	std::vector<std::wstring> out;
	size_t start = 0;
	while (start <= s.size())
	{
		const size_t pos = s.find(delim, start);
		if (pos == std::wstring::npos)
		{
			out.push_back(s.substr(start));
			break;
		}
		out.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}
	return out;
}

static int LevenshteinDistance(const std::wstring& a, const std::wstring& b)
{
	const size_t n = a.size();
	const size_t m = b.size();
	if (n == 0)
	{
		return static_cast<int>(m);
	}
	if (m == 0)
	{
		return static_cast<int>(n);
	}
	std::vector<int> prev(m + 1);
	std::vector<int> cur(m + 1);
	for (size_t j = 0; j <= m; ++j)
	{
		prev[j] = static_cast<int>(j);
	}
	for (size_t i = 1; i <= n; ++i)
	{
		cur[0] = static_cast<int>(i);
		for (size_t j = 1; j <= m; ++j)
		{
			const int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
			cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
		}
		std::swap(prev, cur);
	}
	return prev[m];
}

static int FuzzyScore0To100(const std::wstring& a, const std::wstring& b)
{
	const std::wstring aa = Trim(a);
	const std::wstring bb = Trim(b);
	const size_t maxLen = std::max(aa.size(), bb.size());
	if (maxLen == 0)
	{
		return 0;
	}
	const int dist = LevenshteinDistance(aa, bb);
	const double score = 1.0 - (static_cast<double>(dist) / static_cast<double>(maxLen));
	int s = static_cast<int>(score * 100.0 + 0.5);
	if (s < 0) s = 0;
	if (s > 100) s = 100;
	return s;
}

static void ReplaceAll(std::wstring& s, const std::wstring& from, const std::wstring& to)
{
	if (from.empty())
	{
		return;
	}
	size_t pos = 0;
	while ((pos = s.find(from, pos)) != std::wstring::npos)
	{
		s.replace(pos, from.size(), to);
		pos += to.size();
	}
}

static std::wstring ReplyTextToEdit(std::wstring s)
{
	ReplaceAll(s, L"\r\n", L"\n");
	ReplaceAll(s, L"\r", L"\n");
	ReplaceAll(s, L"{nl}", L"\n");
	ReplaceAll(s, L"\n", L"\r\n");
	return s;
}

static std::wstring ReplyTextFromEdit(std::wstring s)
{
	ReplaceAll(s, L"\r\n", L"\n");
	ReplaceAll(s, L"\r", L"");
	ReplaceAll(s, L"\n", L"{nl}");
	return s;
}

static std::wstring IniEscape(std::wstring s)
{
	ReplaceAll(s, L"\\", L"\\\\");
	ReplaceAll(s, L"\r", L"\\r");
	ReplaceAll(s, L"\n", L"\\n");
	ReplaceAll(s, L"[", L"\\[");
	ReplaceAll(s, L"]", L"\\]");
	return s;
}

static std::wstring IniUnescape(const std::wstring& s)
{
	std::wstring out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i)
	{
		const wchar_t c = s[i];
		if (c != L'\\' || i + 1 >= s.size())
		{
			out.push_back(c);
			continue;
		}
		const wchar_t n = s[i + 1];
		if (n == L'\\')
		{
			out.push_back(L'\\');
			++i;
			continue;
		}
		if (n == L'n')
		{
			out.push_back(L'\n');
			++i;
			continue;
		}
		if (n == L'r')
		{
			out.push_back(L'\r');
			++i;
			continue;
		}
		if (n == L'[')
		{
			out.push_back(L'[');
			++i;
			continue;
		}
		if (n == L']')
		{
			out.push_back(L']');
			++i;
			continue;
		}
		out.push_back(c);
	}
	return out;
}

static std::wstring GetEditText(HWND hEdit);
static void SetEditText(HWND hEdit, const std::wstring& s);

static std::wstring ApplyRegexGroups(std::wstring tpl, const std::wsmatch& m)
{
	for (int i = 1; i <= 9; ++i)
	{
		if (static_cast<size_t>(i) >= m.size())
		{
			break;
		}
		const std::wstring key = L"$" + std::to_wstring(i);
		ReplaceAll(tpl, key, m[i].str());
	}
	return tpl;
}

static std::wstring GetPluginDataDirW()
{
	std::string dirUtf8 = xlz::GetPluginDataDirectory(g_sdk);
	if (dirUtf8.empty())
	{
		return std::wstring();
	}
	std::wstring dir = Utf8ToWide(dirUtf8);
	std::replace(dir.begin(), dir.end(), L'/', L'\\');
	if (!dir.empty() && dir.back() != L'\\')
	{
		dir.push_back(L'\\');
	}
	return dir;
}

static std::wstring GetConfigPathW()
{
	if (!g_configPath.empty())
	{
		return g_configPath;
	}
	const std::wstring dir = GetPluginDataDirW();
	if (dir.empty())
	{
		return std::wstring();
	}
	EnsureDirectoryRecursiveW(dir);
	g_configPath = dir + L"keyword_reply.ini";
	EnsureUnicodeIniFileW(g_configPath);
	return g_configPath;
}

static Config DefaultConfig()
{
	Config c;
	c.enabled = true;
	{
		Rule r;
		r.enabled = true;
		r.type = MatchType::Exact;
		r.pattern = L"ping";
		r.replies = { L"pong" };
		c.rules.push_back(std::move(r));
	}
	{
		Rule r;
		r.enabled = true;
		r.type = MatchType::Contains;
		r.pattern = L"你好";
		r.replies = { L"你好呀", L"哈喽" };
		c.rules.push_back(std::move(r));
	}
	{
		Rule r;
		r.enabled = true;
		r.type = MatchType::Fuzzy;
		r.pattern = L"早上好";
		r.fuzzyThreshold = 70;
		r.replies = { L"早", L"早安" };
		c.rules.push_back(std::move(r));
	}
	{
		Rule r;
		r.enabled = true;
		r.type = MatchType::Regex;
		r.pattern = L"^/echo\\s+(.+)$";
		r.replies = { L"$1" };
		c.rules.push_back(std::move(r));
	}
	return c;
}

static bool LoadConfigFromDisk(Config& out)
{
	const std::wstring iniPath = GetConfigPathW();
	if (iniPath.empty())
	{
		return false;
	}
	const int enabled = ::GetPrivateProfileIntW(L"General", L"Enabled", 1, iniPath.c_str());
	const int ruleCount = ::GetPrivateProfileIntW(L"General", L"RuleCount", 0, iniPath.c_str());
	out.enabled = enabled != 0;
	out.rules.clear();
	for (int i = 0; i < ruleCount; ++i)
	{
		const std::wstring section = L"Rule" + std::to_wstring(i);
		const int ren = ::GetPrivateProfileIntW(section.c_str(), L"Enabled", 1, iniPath.c_str());
		const std::wstring type = ReadIniStringW(iniPath, section, L"Type", L"contains");
		const std::wstring pattern = IniUnescape(ReadIniStringW(iniPath, section, L"Pattern", L""));
		const int threshold = ::GetPrivateProfileIntW(section.c_str(), L"FuzzyThreshold", 75, iniPath.c_str());

		Rule r;
		r.enabled = ren != 0;
		r.type = MatchTypeFromString(type);
		r.pattern = pattern;
		r.fuzzyThreshold = threshold;
		const int replyCount = ::GetPrivateProfileIntW(section.c_str(), L"ReplyCount", 0, iniPath.c_str());
		if (replyCount > 0)
		{
			for (int j = 0; j < replyCount; ++j)
			{
				const std::wstring key = L"Reply" + std::to_wstring(j);
				const std::wstring v = IniUnescape(ReadIniStringW(iniPath, section, key, L""));
				if (!v.empty())
				{
					r.replies.push_back(v);
				}
			}
		}
		else
		{
			const std::wstring repliesLegacy = ReadIniStringW(iniPath, section, L"Replies", L"");
			for (auto& p : Split(repliesLegacy, L';'))
			{
				const std::wstring t = Trim(IniUnescape(p));
				if (!t.empty())
				{
					r.replies.push_back(t);
				}
			}
		}
		if (!Trim(r.pattern).empty() && !r.replies.empty())
		{
			out.rules.push_back(std::move(r));
		}
	}
	if (out.rules.empty())
	{
		out = DefaultConfig();
		return false;
	}
	return true;
}

static void SaveConfigToDisk(const Config& cfg)
{
	const std::wstring iniPath = GetConfigPathW();
	if (iniPath.empty())
	{
		return;
	}
	const int oldCount = ::GetPrivateProfileIntW(L"General", L"RuleCount", 0, iniPath.c_str());

	::WritePrivateProfileStringW(L"General", L"Enabled", cfg.enabled ? L"1" : L"0", iniPath.c_str());
	::WritePrivateProfileStringW(L"General", L"RuleCount", std::to_wstring(cfg.rules.size()).c_str(), iniPath.c_str());

	for (size_t i = 0; i < cfg.rules.size(); ++i)
	{
		const Rule& r = cfg.rules[i];
		const std::wstring section = L"Rule" + std::to_wstring(i);

		::WritePrivateProfileStringW(section.c_str(), L"Enabled", r.enabled ? L"1" : L"0", iniPath.c_str());
		::WritePrivateProfileStringW(section.c_str(), L"Type", MatchTypeToString(r.type).c_str(), iniPath.c_str());
		const std::wstring patternEsc = IniEscape(r.pattern);
		::WritePrivateProfileStringW(section.c_str(), L"Pattern", patternEsc.c_str(), iniPath.c_str());
		::WritePrivateProfileStringW(section.c_str(), L"ReplyCount", std::to_wstring(r.replies.size()).c_str(), iniPath.c_str());
		for (size_t j = 0; j < r.replies.size(); ++j)
		{
			const std::wstring key = L"Reply" + std::to_wstring(j);
			const std::wstring val = IniEscape(r.replies[j]);
			::WritePrivateProfileStringW(section.c_str(), key.c_str(), val.c_str(), iniPath.c_str());
		}
		::WritePrivateProfileStringW(section.c_str(), L"Replies", nullptr, iniPath.c_str());
		::WritePrivateProfileStringW(section.c_str(), L"FuzzyThreshold", std::to_wstring(r.fuzzyThreshold).c_str(), iniPath.c_str());
	}

	for (int i = static_cast<int>(cfg.rules.size()); i < oldCount; ++i)
	{
		const std::wstring section = L"Rule" + std::to_wstring(i);
		::WritePrivateProfileStringW(section.c_str(), nullptr, nullptr, iniPath.c_str());
	}
}

static std::wstring SerializeConfigToText(const Config& cfg)
{
	std::wstring out;
	out += L"enabled=";
	out += cfg.enabled ? L"1" : L"0";
	out += L"\r\n";
	for (const Rule& r : cfg.rules)
	{
		out += L"rule|enabled=";
		out += r.enabled ? L"1" : L"0";
		out += L"|type=";
		out += MatchTypeToString(r.type);
		out += L"|pattern=";
		out += r.pattern;
		out += L"|replies=";
		for (size_t i = 0; i < r.replies.size(); ++i)
		{
			if (i > 0)
			{
				out += L";";
			}
			out += r.replies[i];
		}
		out += L"|threshold=";
		out += std::to_wstring(r.fuzzyThreshold);
		out += L"\r\n";
	}
	return out;
}

static bool TryParseInt(const std::wstring& s, int& out)
{
	try
	{
		size_t idx = 0;
		const int v = std::stoi(s, &idx, 10);
		if (idx != s.size())
		{
			return false;
		}
		out = v;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

static bool ParseConfigText(const std::wstring& text, Config& cfg, std::wstring& err)
{
	cfg.enabled = true;
	cfg.rules.clear();
	std::vector<std::wstring> lines = Split(text, L'\n');
	int lineNo = 0;
	for (auto& raw : lines)
	{
		++lineNo;
		std::wstring line = raw;
		if (!line.empty() && line.back() == L'\r')
		{
			line.pop_back();
		}
		line = Trim(line);
		if (line.empty())
		{
			continue;
		}
		if (line[0] == L'#' || line[0] == L';')
		{
			continue;
		}

		if (line.rfind(L"enabled=", 0) == 0)
		{
			const std::wstring v = Trim(line.substr(8));
			cfg.enabled = (v == L"1" || v == L"true" || v == L"TRUE");
			continue;
		}

		if (line.rfind(L"rule|", 0) != 0)
		{
			err = L"第 " + std::to_wstring(lineNo) + L" 行格式不支持";
			return false;
		}

		Rule r;
		r.enabled = true;
		r.type = MatchType::Contains;
		r.fuzzyThreshold = 75;

		const std::vector<std::wstring> parts = Split(line, L'|');
		for (size_t i = 1; i < parts.size(); ++i)
		{
			const std::wstring part = parts[i];
			const size_t eq = part.find(L'=');
			if (eq == std::wstring::npos)
			{
				continue;
			}
			const std::wstring k = Trim(part.substr(0, eq));
			const std::wstring v = part.substr(eq + 1);
			if (k == L"enabled")
			{
				r.enabled = (Trim(v) == L"1" || Trim(v) == L"true" || Trim(v) == L"TRUE");
			}
			else if (k == L"type")
			{
				r.type = MatchTypeFromString(Trim(v));
			}
			else if (k == L"pattern")
			{
				r.pattern = v;
			}
			else if (k == L"replies")
			{
				r.replies.clear();
				for (auto& rp : Split(v, L';'))
				{
					const std::wstring t = Trim(rp);
					if (!t.empty())
					{
						r.replies.push_back(t);
					}
				}
			}
			else if (k == L"threshold")
			{
				int th = 75;
				if (TryParseInt(Trim(v), th))
				{
					if (th < 0) th = 0;
					if (th > 100) th = 100;
					r.fuzzyThreshold = th;
				}
			}
		}

		if (Trim(r.pattern).empty())
		{
			err = L"第 " + std::to_wstring(lineNo) + L" 行 pattern 为空";
			return false;
		}
		if (r.replies.empty())
		{
			err = L"第 " + std::to_wstring(lineNo) + L" 行 replies 为空";
			return false;
		}
		cfg.rules.push_back(std::move(r));
	}
	if (cfg.rules.empty())
	{
		err = L"未配置任何 rule";
		return false;
	}
	return true;
}

static std::wstring PickRandomReply(const Rule& r)
{
	if (r.replies.empty())
	{
		return std::wstring();
	}
	if (r.replies.size() == 1)
	{
		return r.replies[0];
	}
	static thread_local std::mt19937 rng(static_cast<uint32_t>(::GetTickCount()));
	std::uniform_int_distribution<size_t> dist(0, r.replies.size() - 1);
	return r.replies[dist(rng)];
}

static bool MatchRule(const Rule& r, const std::wstring& content, std::wsmatch& outMatch)
{
	const std::wstring c = Trim(content);
	const std::wstring p = Trim(r.pattern);
	switch (r.type)
	{
	case MatchType::Exact:
		return c == p;
	case MatchType::Contains:
		return c.find(p) != std::wstring::npos;
	case MatchType::Fuzzy:
		return FuzzyScore0To100(c, p) >= r.fuzzyThreshold;
	case MatchType::Regex:
		try
		{
			const std::wregex re(p, std::regex_constants::ECMAScript);
			return std::regex_search(c, outMatch, re);
		}
		catch (...)
		{
			return false;
		}
	}
	return false;
}

static std::wstring Usc2AnsiPtrToW(const char* s)
{
	return Utf8ToWide(xlz::Usc2AnsiToUtf8(s ? s : ""));
}

static void ApplyReplyVariables(std::wstring& reply, const xlz::GroupMessageEvent& ev, const std::wstring& messageContent)
{
	ReplaceAll(reply, L"{thisQQ}", std::to_wstring(static_cast<long long>(ev.ThisQQ)));
	ReplaceAll(reply, L"{groupQQ}", std::to_wstring(static_cast<long long>(ev.MessageGroupQQ)));
	ReplaceAll(reply, L"{senderQQ}", std::to_wstring(static_cast<long long>(ev.SenderQQ)));
	ReplaceAll(reply, L"{senderNick}", Usc2AnsiPtrToW(ev.SenderNickname));
	ReplaceAll(reply, L"{groupName}", Usc2AnsiPtrToW(ev.SourceGroupName));
	ReplaceAll(reply, L"{message}", messageContent);
	ReplaceAll(reply, L"{nl}", L"\n");
}

struct SettingsWindowState
{
	HWND hwnd = nullptr;
	Config working;
	int selectedRule = -1;
	int selectedReply = -1;

	HBRUSH brushBg = nullptr;
	HBRUSH brushEdit = nullptr;
	HBRUSH brushPanel = nullptr;

	HWND chkGlobalEnabled = nullptr;
	HWND txtGlobalEnabled = nullptr;

	HWND lstRules = nullptr;
	HWND btnRuleAdd = nullptr;
	HWND btnRuleDel = nullptr;
	HWND btnRuleUp = nullptr;
	HWND btnRuleDown = nullptr;

	HWND chkRuleEnabled = nullptr;
	HWND txtRuleEnabled = nullptr;
	HWND cmbType = nullptr;
	HWND edtPattern = nullptr;
	HWND edtThreshold = nullptr;

	HWND lstReplies = nullptr;
	HWND edtReply = nullptr;
	HWND btnReplyAdd = nullptr;
	HWND btnReplyUpdate = nullptr;
	HWND btnReplyDel = nullptr;

	HWND edtVars = nullptr;

	HWND btnSave = nullptr;
	HWND btnReload = nullptr;
	HWND btnClose = nullptr;
};

static const wchar_t* kSettingsWndClass = L"XlzKeywordReplySettingsWnd";

enum : int
{
	IDC_GLOBAL_ENABLED = 2001,
	IDC_RULE_LIST = 2002,
	IDC_RULE_ADD = 2003,
	IDC_RULE_DEL = 2004,
	IDC_RULE_UP = 2005,
	IDC_RULE_DOWN = 2006,

	IDC_RULE_ENABLED = 2010,
	IDC_RULE_TYPE = 2011,
	IDC_RULE_PATTERN = 2012,
	IDC_RULE_THRESHOLD = 2013,

	IDC_REPLY_LIST = 2020,
	IDC_REPLY_EDIT = 2021,
	IDC_REPLY_ADD = 2022,
	IDC_REPLY_UPDATE = 2023,
	IDC_REPLY_DEL = 2024,

	IDC_VARS_EDIT = 2030,

	IDC_BTN_SAVE = 2040,
	IDC_BTN_RELOAD = 2041,
	IDC_BTN_CLOSE = 2042,
};

static MatchType GetMatchTypeFromCombo(HWND hCombo)
{
	const int idx = static_cast<int>(::SendMessageW(hCombo, CB_GETCURSEL, 0, 0));
	if (idx < 0)
	{
		return MatchType::Contains;
	}
	const LRESULT data = ::SendMessageW(hCombo, CB_GETITEMDATA, static_cast<WPARAM>(idx), 0);
	if (data == static_cast<LRESULT>(MatchType::Exact)) return MatchType::Exact;
	if (data == static_cast<LRESULT>(MatchType::Contains)) return MatchType::Contains;
	if (data == static_cast<LRESULT>(MatchType::Fuzzy)) return MatchType::Fuzzy;
	if (data == static_cast<LRESULT>(MatchType::Regex)) return MatchType::Regex;
	return MatchType::Contains;
}

static void SetMatchTypeToCombo(HWND hCombo, MatchType t)
{
	const int count = static_cast<int>(::SendMessageW(hCombo, CB_GETCOUNT, 0, 0));
	for (int i = 0; i < count; ++i)
	{
		const LRESULT data = ::SendMessageW(hCombo, CB_GETITEMDATA, static_cast<WPARAM>(i), 0);
		if (data == static_cast<LRESULT>(t))
		{
			::SendMessageW(hCombo, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
			return;
		}
	}
	::SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
}

static int GetIntFromEdit(HWND hEdit, int defaultValue)
{
	const std::wstring s = Trim(GetEditText(hEdit));
	if (s.empty())
	{
		return defaultValue;
	}
	int v = defaultValue;
	if (!TryParseInt(s, v))
	{
		return defaultValue;
	}
	return v;
}

static std::wstring RuleListText(const Rule& r, int index)
{
	std::wstring t = std::to_wstring(index + 1);
	t += L". ";
	t += r.enabled ? L"[√] " : L"[×] ";
	switch (r.type)
	{
	case MatchType::Exact: t += L"精确 "; break;
	case MatchType::Contains: t += L"包含 "; break;
	case MatchType::Fuzzy: t += L"模糊 "; break;
	case MatchType::Regex: t += L"正则 "; break;
	}
	t += r.pattern;
	return t;
}

static void RefreshRuleList(SettingsWindowState* st)
{
	::SendMessageW(st->lstRules, LB_RESETCONTENT, 0, 0);
	for (size_t i = 0; i < st->working.rules.size(); ++i)
	{
		const std::wstring text = RuleListText(st->working.rules[i], static_cast<int>(i));
		::SendMessageW(st->lstRules, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
	}
}

static void RefreshReplyList(SettingsWindowState* st)
{
	::SendMessageW(st->lstReplies, LB_RESETCONTENT, 0, 0);
	if (st->selectedRule < 0 || st->selectedRule >= static_cast<int>(st->working.rules.size()))
	{
		return;
	}
	const Rule& r = st->working.rules[st->selectedRule];
	for (size_t i = 0; i < r.replies.size(); ++i)
	{
		std::wstring t = std::to_wstring(i + 1);
		t += L". ";
		std::wstring one = r.replies[i];
		ReplaceAll(one, L"{nl}", L"\\n");
		ReplaceAll(one, L"\r", L"");
		ReplaceAll(one, L"\n", L"\\n");
		t += one;
		::SendMessageW(st->lstReplies, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
	}
}

static void LoadRuleToUI(SettingsWindowState* st, int index)
{
	st->selectedReply = -1;
	::SendMessageW(st->lstReplies, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
	SetEditText(st->edtReply, L"");

	if (index < 0 || index >= static_cast<int>(st->working.rules.size()))
	{
		::SendMessageW(st->chkRuleEnabled, BM_SETCHECK, BST_UNCHECKED, 0);
		SetMatchTypeToCombo(st->cmbType, MatchType::Contains);
		SetEditText(st->edtPattern, L"");
		SetEditText(st->edtThreshold, L"75");
		RefreshReplyList(st);
		return;
	}
	const Rule& r = st->working.rules[index];
	::SendMessageW(st->chkRuleEnabled, BM_SETCHECK, r.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
	SetMatchTypeToCombo(st->cmbType, r.type);
	SetEditText(st->edtPattern, r.pattern);
	SetEditText(st->edtThreshold, std::to_wstring(r.fuzzyThreshold));
	RefreshReplyList(st);
}

static void SaveUIToRule(SettingsWindowState* st, int index)
{
	if (index < 0 || index >= static_cast<int>(st->working.rules.size()))
	{
		return;
	}
	Rule& r = st->working.rules[index];
	r.enabled = (::SendMessageW(st->chkRuleEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
	r.type = GetMatchTypeFromCombo(st->cmbType);
	r.pattern = GetEditText(st->edtPattern);
	int th = GetIntFromEdit(st->edtThreshold, 75);
	if (th < 0) th = 0;
	if (th > 100) th = 100;
	r.fuzzyThreshold = th;
}

static std::wstring ValidateConfigForSave(const Config& cfg)
{
	if (cfg.rules.empty())
	{
		return L"关键词列表为空";
	}
	for (size_t i = 0; i < cfg.rules.size(); ++i)
	{
		const Rule& r = cfg.rules[i];
		if (Trim(r.pattern).empty())
		{
			return L"第 " + std::to_wstring(i + 1) + L" 条关键词 pattern 为空";
		}
		if (r.replies.empty())
		{
			return L"第 " + std::to_wstring(i + 1) + L" 条关键词 replies 为空";
		}
	}
	return L"";
}

static void LayoutSettingsControls(SettingsWindowState* st, int width, int height)
{
	const int padding = 20;
	const int btnH = 32;
	const int gap = 12;
	const int leftW = 300;
	const int varsW = 280;
	const int bottomH = btnH + padding;
	const int clientH = std::max(10, height);
	const int contentH = std::max(10, clientH - bottomH - padding * 2);
	const int headerH = 19;
	const int headerGapY = 11;
	const int chkW = 16;
	const int chkH = 16;

	::MoveWindow(st->chkGlobalEnabled, padding, padding + (headerH - chkH) / 2, chkW, chkH, TRUE);
	::MoveWindow(st->txtGlobalEnabled, padding + 20, padding, 180, headerH, TRUE);

	const int listY = padding + headerH + headerGapY;
	const int bottomY = height - padding - btnH;
	const int replyBtnY = bottomY - btnH - gap;
	const int btnY = replyBtnY;
	const int listH = std::max(10, btnY - gap - listY);
	::MoveWindow(st->lstRules, padding, listY, leftW, listH, TRUE);

	const int smallBtnW = (leftW - gap * 3) / 4;
	::MoveWindow(st->btnRuleAdd, padding, btnY, smallBtnW, btnH, TRUE);
	::MoveWindow(st->btnRuleDel, padding + (smallBtnW + gap) * 1, btnY, smallBtnW, btnH, TRUE);
	::MoveWindow(st->btnRuleUp, padding + (smallBtnW + gap) * 2, btnY, smallBtnW, btnH, TRUE);
	::MoveWindow(st->btnRuleDown, padding + (smallBtnW + gap) * 3, btnY, smallBtnW, btnH, TRUE);

	const int rightX = padding + leftW + padding;
	const int varsX = width - padding - varsW;
	const int rightW = std::max(10, varsX - rightX - padding);

	int y = padding;
	::MoveWindow(st->chkRuleEnabled, rightX, y + (headerH - chkH) / 2, chkW, chkH, TRUE);
	::MoveWindow(st->txtRuleEnabled, rightX + 20, y, 120, headerH, TRUE);
	y += headerH + headerGapY;

	::MoveWindow(st->cmbType, rightX, y, 120, 28, TRUE);
	::MoveWindow(st->edtThreshold, rightX + 130, y, 70, 28, TRUE);
	y += 40;

	::MoveWindow(st->edtPattern, rightX, y, rightW, 28, TRUE);
	y += 40;

	const int replyListH = 140;
	::MoveWindow(st->lstReplies, rightX, y, rightW, replyListH, TRUE);
	y += replyListH + gap;

	const int replyEditH = std::max(60, replyBtnY - gap - y);
	::MoveWindow(st->edtReply, rightX, y, rightW, replyEditH, TRUE);

	const int replyBtnW = (rightW - gap * 2) / 3;
	::MoveWindow(st->btnReplyAdd, rightX, replyBtnY, replyBtnW, btnH, TRUE);
	::MoveWindow(st->btnReplyUpdate, rightX + replyBtnW + gap, replyBtnY, replyBtnW, btnH, TRUE);
	::MoveWindow(st->btnReplyDel, rightX + (replyBtnW + gap) * 2, replyBtnY, replyBtnW, btnH, TRUE);

	::MoveWindow(st->edtVars, varsX, padding, varsW, contentH, TRUE);

	const int btnW = 120;
	::MoveWindow(st->btnClose, width - padding - btnW, bottomY, btnW, btnH, TRUE);
	::MoveWindow(st->btnReload, width - padding - btnW * 2 - gap, bottomY, btnW, btnH, TRUE);
	::MoveWindow(st->btnSave, width - padding - btnW * 3 - gap * 2, bottomY, btnW, btnH, TRUE);
}

static std::wstring GetEditText(HWND hEdit)
{
	const int len = ::GetWindowTextLengthW(hEdit);
	if (len <= 0)
	{
		return std::wstring();
	}
	std::wstring s;
	s.resize(static_cast<size_t>(len) + 1);
	::GetWindowTextW(hEdit, s.data(), len + 1);
	s.resize(std::wcslen(s.c_str()));
	return s;
}

static void SetEditText(HWND hEdit, const std::wstring& s)
{
	::SetWindowTextW(hEdit, s.c_str());
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	SettingsWindowState* st = reinterpret_cast<SettingsWindowState*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg)
	{
	case WM_NCCREATE:
	{
		const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
		::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
		return TRUE;
	}
	case WM_CREATE:
	{
		st = reinterpret_cast<SettingsWindowState*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (st == nullptr)
		{
			return -1;
		}
		st->hwnd = hwnd;
		::SetWindowTextW(hwnd, L"Main");
		st->brushBg = ::CreateSolidBrush(RGB(18, 18, 18));
		st->brushEdit = ::CreateSolidBrush(RGB(28, 28, 28));
		st->brushPanel = ::CreateSolidBrush(RGB(18, 18, 18));

		{
			std::lock_guard<std::mutex> lock(g_configMutex);
			st->working = g_config;
		}

		st->chkGlobalEnabled = ::CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_GLOBAL_ENABLED), nullptr, nullptr);
		st->txtGlobalEnabled = ::CreateWindowExW(0, L"STATIC", L"启用关键词回复", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
		st->lstRules = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_LIST), nullptr, nullptr);
		st->btnRuleAdd = ::CreateWindowExW(0, L"BUTTON", L"新增", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_ADD), nullptr, nullptr);
		st->btnRuleDel = ::CreateWindowExW(0, L"BUTTON", L"删除", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_DEL), nullptr, nullptr);
		st->btnRuleUp = ::CreateWindowExW(0, L"BUTTON", L"上移", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_UP), nullptr, nullptr);
		st->btnRuleDown = ::CreateWindowExW(0, L"BUTTON", L"下移", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_DOWN), nullptr, nullptr);

		st->chkRuleEnabled = ::CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_ENABLED), nullptr, nullptr);
		st->txtRuleEnabled = ::CreateWindowExW(0, L"STATIC", L"启用本条", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
		st->cmbType = ::CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_TYPE), nullptr, nullptr);
		st->edtThreshold = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"75", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_THRESHOLD), nullptr, nullptr);
		st->edtPattern = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_RULE_PATTERN), nullptr, nullptr);

		st->lstReplies = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_REPLY_LIST), nullptr, nullptr);
		st->edtReply = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_REPLY_EDIT), nullptr, nullptr);
		st->btnReplyAdd = ::CreateWindowExW(0, L"BUTTON", L"新增回复", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_REPLY_ADD), nullptr, nullptr);
		st->btnReplyUpdate = ::CreateWindowExW(0, L"BUTTON", L"更新回复", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_REPLY_UPDATE), nullptr, nullptr);
		st->btnReplyDel = ::CreateWindowExW(0, L"BUTTON", L"删除回复", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_REPLY_DEL), nullptr, nullptr);

		st->edtVars = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_VARS_EDIT), nullptr, nullptr);

		st->btnSave = ::CreateWindowExW(0, L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_SAVE), nullptr, nullptr);
		st->btnReload = ::CreateWindowExW(0, L"BUTTON", L"重新加载", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_RELOAD), nullptr, nullptr);
		st->btnClose = ::CreateWindowExW(0, L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(IDC_BTN_CLOSE), nullptr, nullptr);

		const HFONT font = reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
		HWND controls[] = {
			st->chkGlobalEnabled, st->txtGlobalEnabled, st->lstRules, st->btnRuleAdd, st->btnRuleDel, st->btnRuleUp, st->btnRuleDown,
			st->chkRuleEnabled, st->txtRuleEnabled, st->cmbType, st->edtThreshold, st->edtPattern,
			st->lstReplies, st->edtReply, st->btnReplyAdd, st->btnReplyUpdate, st->btnReplyDel,
			st->edtVars,
			st->btnSave, st->btnReload, st->btnClose
		};
		for (HWND h : controls)
		{
			if (h != nullptr)
			{
				::SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
			}
		}

		const int iExact = static_cast<int>(::SendMessageW(st->cmbType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"精确")));
		::SendMessageW(st->cmbType, CB_SETITEMDATA, static_cast<WPARAM>(iExact), static_cast<LPARAM>(MatchType::Exact));
		const int iContains = static_cast<int>(::SendMessageW(st->cmbType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"包含")));
		::SendMessageW(st->cmbType, CB_SETITEMDATA, static_cast<WPARAM>(iContains), static_cast<LPARAM>(MatchType::Contains));
		const int iFuzzy = static_cast<int>(::SendMessageW(st->cmbType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"模糊")));
		::SendMessageW(st->cmbType, CB_SETITEMDATA, static_cast<WPARAM>(iFuzzy), static_cast<LPARAM>(MatchType::Fuzzy));
		const int iRegex = static_cast<int>(::SendMessageW(st->cmbType, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"正则")));
		::SendMessageW(st->cmbType, CB_SETITEMDATA, static_cast<WPARAM>(iRegex), static_cast<LPARAM>(MatchType::Regex));

		::SendMessageW(st->chkGlobalEnabled, BM_SETCHECK, st->working.enabled ? BST_CHECKED : BST_UNCHECKED, 0);

		std::wstring vars;
		vars += L"回复变量：\r\n";
		vars += L"{thisQQ}    机器人QQ\r\n";
		vars += L"{groupQQ}   群号\r\n";
		vars += L"{senderQQ}  发送者QQ\r\n";
		vars += L"{senderNick}发送者昵称\r\n";
		vars += L"{groupName} 群名\r\n";
		vars += L"{message}   原消息\r\n";
		vars += L"{nl}        换行\r\n";
		vars += L"\r\n正则回复：支持 $1..$9\r\n";
		SetEditText(st->edtVars, vars);

		RefreshRuleList(st);
		if (!st->working.rules.empty())
		{
			st->selectedRule = 0;
			::SendMessageW(st->lstRules, LB_SETCURSEL, 0, 0);
			LoadRuleToUI(st, 0);
		}
		else
		{
			st->selectedRule = -1;
			LoadRuleToUI(st, -1);
		}
		return 0;
	}
	case WM_SIZE:
	{
		if (st != nullptr)
		{
			const int w = LOWORD(lParam);
			const int h = HIWORD(lParam);
			LayoutSettingsControls(st, w, h);
		}
		return 0;
	}
	case WM_COMMAND:
		{
			if (st == nullptr)
			{
				return 0;
			}
			const int id = LOWORD(wParam);
			const int code = HIWORD(wParam);
			
			// 处理文本控件点击，切换对应的复选框
			if (code == STN_CLICKED)
			{
				HWND hStatic = reinterpret_cast<HWND>(lParam);
				if (hStatic == st->txtGlobalEnabled)
				{
					::SendMessageW(st->chkGlobalEnabled, BM_CLICK, 0, 0);
					return 0;
				}
				if (hStatic == st->txtRuleEnabled)
				{
					::SendMessageW(st->chkRuleEnabled, BM_CLICK, 0, 0);
					return 0;
				}
			}

		if (id == IDC_RULE_LIST && code == LBN_SELCHANGE)
		{
			SaveUIToRule(st, st->selectedRule);
			const int sel = static_cast<int>(::SendMessageW(st->lstRules, LB_GETCURSEL, 0, 0));
			st->selectedRule = sel;
			LoadRuleToUI(st, sel);
			return 0;
		}
		if (id == IDC_REPLY_LIST && code == LBN_SELCHANGE)
		{
			const int sel = static_cast<int>(::SendMessageW(st->lstReplies, LB_GETCURSEL, 0, 0));
			st->selectedReply = sel;
			if (st->selectedRule >= 0 && st->selectedRule < static_cast<int>(st->working.rules.size()) && sel >= 0)
			{
				const Rule& r = st->working.rules[st->selectedRule];
				if (sel < static_cast<int>(r.replies.size()))
				{
					SetEditText(st->edtReply, ReplyTextToEdit(r.replies[sel]));
				}
			}
			return 0;
		}

		if (id == IDC_BTN_CLOSE)
		{
			::DestroyWindow(hwnd);
			return 0;
		}
		if (id == IDC_BTN_RELOAD)
		{
			Config cfg;
			LoadConfigFromDisk(cfg);
			st->working = cfg;
			::SendMessageW(st->chkGlobalEnabled, BM_SETCHECK, st->working.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
			RefreshRuleList(st);
			st->selectedRule = st->working.rules.empty() ? -1 : 0;
			::SendMessageW(st->lstRules, LB_SETCURSEL, st->selectedRule >= 0 ? 0 : static_cast<WPARAM>(-1), 0);
			LoadRuleToUI(st, st->selectedRule);
			return 0;
		}
		if (id == IDC_BTN_SAVE)
		{
			SaveUIToRule(st, st->selectedRule);
			st->working.enabled = (::SendMessageW(st->chkGlobalEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
			const std::wstring err = ValidateConfigForSave(st->working);
			if (!err.empty())
			{
				::MessageBoxW(hwnd, err.c_str(), L"配置错误", MB_ICONERROR | MB_OK);
				return 0;
			}
			{
				std::lock_guard<std::mutex> lock(g_configMutex);
				g_config = st->working;
			}
			SaveConfigToDisk(st->working);
			::MessageBoxW(hwnd, L"已保存", L"关键词回复", MB_ICONINFORMATION | MB_OK);
			return 0;
		}

		if (id == IDC_RULE_ADD)
		{
			SaveUIToRule(st, st->selectedRule);
			Rule r;
			r.enabled = true;
			r.type = MatchType::Contains;
			r.pattern = L"";
			r.fuzzyThreshold = 75;
			r.replies = { L"" };
			st->working.rules.push_back(std::move(r));
			RefreshRuleList(st);
			st->selectedRule = static_cast<int>(st->working.rules.size()) - 1;
			::SendMessageW(st->lstRules, LB_SETCURSEL, static_cast<WPARAM>(st->selectedRule), 0);
			LoadRuleToUI(st, st->selectedRule);
			return 0;
		}
		if (id == IDC_RULE_DEL)
		{
			if (st->selectedRule >= 0 && st->selectedRule < static_cast<int>(st->working.rules.size()))
			{
				st->working.rules.erase(st->working.rules.begin() + st->selectedRule);
				RefreshRuleList(st);
				if (st->working.rules.empty())
				{
					st->selectedRule = -1;
					::SendMessageW(st->lstRules, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
					LoadRuleToUI(st, -1);
				}
				else
				{
					if (st->selectedRule >= static_cast<int>(st->working.rules.size()))
					{
						st->selectedRule = static_cast<int>(st->working.rules.size()) - 1;
					}
					::SendMessageW(st->lstRules, LB_SETCURSEL, static_cast<WPARAM>(st->selectedRule), 0);
					LoadRuleToUI(st, st->selectedRule);
				}
			}
			return 0;
		}
		if (id == IDC_RULE_UP)
		{
			if (st->selectedRule > 0 && st->selectedRule < static_cast<int>(st->working.rules.size()))
			{
				SaveUIToRule(st, st->selectedRule);
				std::swap(st->working.rules[st->selectedRule], st->working.rules[st->selectedRule - 1]);
				--st->selectedRule;
				RefreshRuleList(st);
				::SendMessageW(st->lstRules, LB_SETCURSEL, static_cast<WPARAM>(st->selectedRule), 0);
				LoadRuleToUI(st, st->selectedRule);
			}
			return 0;
		}
		if (id == IDC_RULE_DOWN)
		{
			if (st->selectedRule >= 0 && st->selectedRule + 1 < static_cast<int>(st->working.rules.size()))
			{
				SaveUIToRule(st, st->selectedRule);
				std::swap(st->working.rules[st->selectedRule], st->working.rules[st->selectedRule + 1]);
				++st->selectedRule;
				RefreshRuleList(st);
				::SendMessageW(st->lstRules, LB_SETCURSEL, static_cast<WPARAM>(st->selectedRule), 0);
				LoadRuleToUI(st, st->selectedRule);
			}
			return 0;
		}

		if (id == IDC_REPLY_ADD)
		{
			if (st->selectedRule < 0 || st->selectedRule >= static_cast<int>(st->working.rules.size()))
			{
				return 0;
			}
			std::wstring v = ReplyTextFromEdit(GetEditText(st->edtReply));
			if (v.empty())
			{
				::MessageBoxW(hwnd, L"回复内容不能为空", L"提示", MB_ICONWARNING | MB_OK);
				return 0;
			}
			st->working.rules[st->selectedRule].replies.push_back(v);
			RefreshReplyList(st);
			return 0;
		}
		if (id == IDC_REPLY_UPDATE)
		{
			if (st->selectedRule < 0 || st->selectedRule >= static_cast<int>(st->working.rules.size()))
			{
				return 0;
			}
			if (st->selectedReply < 0)
			{
				::MessageBoxW(hwnd, L"请选择要更新的回复", L"提示", MB_ICONWARNING | MB_OK);
				return 0;
			}
			Rule& r = st->working.rules[st->selectedRule];
			if (st->selectedReply >= static_cast<int>(r.replies.size()))
			{
				return 0;
			}
			std::wstring v = ReplyTextFromEdit(GetEditText(st->edtReply));
			if (v.empty())
			{
				::MessageBoxW(hwnd, L"回复内容不能为空", L"提示", MB_ICONWARNING | MB_OK);
				return 0;
			}
			r.replies[st->selectedReply] = v;
			RefreshReplyList(st);
			::SendMessageW(st->lstReplies, LB_SETCURSEL, static_cast<WPARAM>(st->selectedReply), 0);
			return 0;
		}
		if (id == IDC_REPLY_DEL)
		{
			if (st->selectedRule < 0 || st->selectedRule >= static_cast<int>(st->working.rules.size()))
			{
				return 0;
			}
			if (st->selectedReply < 0)
			{
				return 0;
			}
			Rule& r = st->working.rules[st->selectedRule];
			if (st->selectedReply >= static_cast<int>(r.replies.size()))
			{
				return 0;
			}
			r.replies.erase(r.replies.begin() + st->selectedReply);
			st->selectedReply = -1;
			SetEditText(st->edtReply, L"");
			RefreshReplyList(st);
			return 0;
		}

		return 0;
	}
	case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORDLG:
		{
			const HDC hdc = reinterpret_cast<HDC>(wParam);
			::SetBkMode(hdc, TRANSPARENT);
			::SetTextColor(hdc, RGB(255, 255, 255));
			return reinterpret_cast<LRESULT>(st ? st->brushBg : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
		}
		case WM_CTLCOLOREDIT:
		{
			const HDC hdc = reinterpret_cast<HDC>(wParam);
			::SetBkMode(hdc, OPAQUE);
			::SetBkColor(hdc, RGB(18, 18, 18));
			::SetTextColor(hdc, RGB(230, 230, 230));
			return reinterpret_cast<LRESULT>(st ? st->brushBg : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
		}
		case WM_CTLCOLORLISTBOX:
		{
			const HDC hdc = reinterpret_cast<HDC>(wParam);
			::SetBkMode(hdc, OPAQUE);
			::SetBkColor(hdc, RGB(18, 18, 18));
			::SetTextColor(hdc, RGB(255, 255, 255));
			return reinterpret_cast<LRESULT>(st ? st->brushBg : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
		}
		case WM_CTLCOLORBTN:
		{
			const HDC hdc = reinterpret_cast<HDC>(wParam);
			::SetBkMode(hdc, OPAQUE);
			::SetBkColor(hdc, RGB(18, 18, 18));
			::SetTextColor(hdc, RGB(255, 255, 255));
			return reinterpret_cast<LRESULT>(st ? st->brushBg : reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
		}
	case WM_CLOSE:
		::DestroyWindow(hwnd);
		return 0;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = ::BeginPaint(hwnd, &ps);
			RECT rc;
			::GetClientRect(hwnd, &rc);
			::FillRect(hdc, &rc, st->brushBg);
			::EndPaint(hwnd, &ps);
			return 0;
		}
	case WM_DESTROY:
		g_settingsHwnd.store(nullptr, std::memory_order_relaxed);
		if (st != nullptr)
		{
			if (st->brushBg) ::DeleteObject(st->brushBg);
			if (st->brushEdit) ::DeleteObject(st->brushEdit);
			if (st->brushPanel) ::DeleteObject(st->brushPanel);
			st->brushBg = nullptr;
			st->brushEdit = nullptr;
			st->brushPanel = nullptr;
		}
		return 0;
	}
	return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void ShowSettingsWindowModal()
{
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = SettingsWndProc;
	wc.hInstance = ::GetModuleHandleW(nullptr);
	wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
	wc.hbrBackground = ::CreateSolidBrush(RGB(18, 18, 18));
	wc.lpszClassName = kSettingsWndClass;
	::RegisterClassExW(&wc);

	SettingsWindowState st;
	HWND hwnd = ::CreateWindowExW(WS_EX_DLGMODALFRAME, kSettingsWndClass, L"Main", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 950, 540, nullptr, nullptr, wc.hInstance, &st);
	if (hwnd == nullptr)
	{
		::DeleteObject(wc.hbrBackground);
		return;
	}
	g_settingsHwnd.store(hwnd, std::memory_order_relaxed);
	::ShowWindow(hwnd, SW_SHOW);
	::UpdateWindow(hwnd);

	MSG msg;
	while (::IsWindow(hwnd) && ::GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		::TranslateMessage(&msg);
		::DispatchMessageW(&msg);
	}

	::UnregisterClassW(kSettingsWndClass, wc.hInstance);
	::DeleteObject(wc.hbrBackground);
}

static int XLZ_CALL HandleGroupMessage(const xlz::GroupMessageEvent* ev)
{
	if (ev == nullptr)
	{
		return 0;
	}
	if (ev->SenderQQ == ev->ThisQQ) {
		return 0; // 忽略自己发送的消息，防止死循环
	}
	Config cfg;
	{
		std::lock_guard<std::mutex> lock(g_configMutex);
		cfg = g_config;
	}
	if (!cfg.enabled)
	{
		return 0;
	}

	const std::wstring content = Utf8ToWide(xlz::GetGroupMessageContentUtf8(*ev));
	for (const Rule& r : cfg.rules)
	{
		if (!r.enabled)
		{
			continue;
		}
		std::wsmatch m;
		if (!MatchRule(r, content, m))
		{
			continue;
		}
		std::wstring reply = PickRandomReply(r);
		if (reply.empty())
		{
			continue;
		}
		if (r.type == MatchType::Regex && m.size() > 1)
		{
			reply = ApplyRegexGroups(reply, m);
		}
		ApplyReplyVariables(reply, *ev, content);
		xlz::SendGroupMessage(g_sdk, ev->ThisQQ, ev->MessageGroupQQ, WideToUtf8(reply).c_str(), false);
		break;
	}

	xlz::ProcessEvents();
	return 0;
}

static int XLZ_CALL HandlePrivateMessage(const xlz::PrivateMessageEvent* ev)
{
	(void)ev;
	xlz::ProcessEvents();
	return 0;
}

static int XLZ_CALL HandleEventMessage(const xlz::EventTypeBase* ev)
{
	(void)ev;
	xlz::ProcessEvents();
	return 0;
}

XLZ_API int XLZ_CALL RecviceGroupMesg(void* data)
{
	return HandleGroupMessage(reinterpret_cast<const xlz::GroupMessageEvent*>(data));
}

XLZ_API int XLZ_CALL RecvicePrivateMsg(void* data)
{
	return HandlePrivateMessage(reinterpret_cast<const xlz::PrivateMessageEvent*>(data));
}

XLZ_API int XLZ_CALL RecviceEventCallBack(void* data)
{
	return HandleEventMessage(reinterpret_cast<const xlz::EventTypeBase*>(data));
}

XLZ_API int XLZ_CALL RotbotAppEnable()
{
	Config cfg;
	const bool loaded = LoadConfigFromDisk(cfg);
	if (!loaded)
	{
		SaveConfigToDisk(cfg);
	}
	{
		std::lock_guard<std::mutex> lock(g_configMutex);
		g_config = cfg;
	}
	xlz::OutputLog(g_sdk, "KeywordReply enabled");
	return 0;
}

XLZ_API int XLZ_CALL AppSetting()
{
	ShowSettingsWindowModal();
	return 0;
}

XLZ_API void XLZ_CALL AppUninstall()
{
}

XLZ_API void XLZ_CALL AppDisabled()
{
	// 释放资源
	xlz::OutputLog(g_sdk, "KeywordReply disabled");
	HWND hwnd = g_settingsHwnd.exchange(nullptr, std::memory_order_relaxed);
	if (hwnd != nullptr && ::IsWindow(hwnd))
	{
		::PostMessageW(hwnd, WM_CLOSE, 0, 0);
	}
	
	// 清理配置路径
	g_configPath.clear();
	
	// 清理配置
	{ 
		std::lock_guard<std::mutex> lock(g_configMutex);
		g_config = Config();
	}
	
	// 清理应用信息
	g_appInfoJsonAcp.clear();
	
	// 注意：g_sdk 不需要手动清理，它的析构函数会自动处理
}

XLZ_API void XLZ_CALL GetSMSVerificationCode(int64_t sourceQQ, void* phone)
{
	(void)sourceQQ;
	(void)phone;
}

XLZ_API void XLZ_CALL SliderRecognition(int64_t sourceQQ, void* url)
{
	(void)sourceQQ;
	(void)url;
}

XLZ_API const char* XLZ_CALL apprun(const char* pluginkey, const char* apidata)
{
	const auto looksLikeApiData = [](const char* s) -> bool
	{
		if (s == nullptr)
		{
			return false;
		}
		if (std::strchr(s, '{') == nullptr || std::strchr(s, ':') == nullptr)
		{
			return false;
		}
		if (std::strstr(s, "output") != nullptr)
		{
			return false;
		}
		return true;
	};

	const char* apiDataJson = nullptr;
	const char* pluginKey = nullptr;
	const size_t len1 = pluginkey ? std::strlen(pluginkey) : 0;
	const size_t len2 = apidata ? std::strlen(apidata) : 0;
	if (!looksLikeApiData(pluginkey) && looksLikeApiData(apidata))
	{
		pluginKey = pluginkey;
		apiDataJson = apidata;
	}
	else if (looksLikeApiData(pluginkey) && !looksLikeApiData(apidata))
	{
		pluginKey = apidata;
		apiDataJson = pluginkey;
	}
	else if (len2 >= len1)
	{
		pluginKey = pluginkey;
		apiDataJson = apidata;
	}
	else
	{
		pluginKey = apidata;
		apiDataJson = pluginkey;
	}
	g_sdk.Initialize(apiDataJson, pluginKey);

	{
		Config cfg;
		const bool loaded = LoadConfigFromDisk(cfg);
		if (!loaded)
		{
			SaveConfigToDisk(cfg);
		}
		std::lock_guard<std::mutex> lock(g_configMutex);
		g_config = cfg;
	}

	xlz::AppInfoBuilder info;
	
	info.SetAppName("Demo 关键词回复 (C++)");
	info.SetAuthor("C++ SDK");
	info.SetAppVersion("1.0.0");
	info.SetDescription("关键词回复：精确/包含/模糊/正则，支持多回复随机与正则分组替换($1..$9)。配置写入取插件数据目录。");

	info.SetPluginEnabledAddress(reinterpret_cast<uintptr_t>(&RotbotAppEnable));
	info.SetPrivateMessageAddress(reinterpret_cast<uintptr_t>(&RecvicePrivateMsg));
	info.SetGroupMessageAddress(reinterpret_cast<uintptr_t>(&RecviceGroupMesg));
	info.SetEventMessageAddress(reinterpret_cast<uintptr_t>(&RecviceEventCallBack));
	info.SetPluginSettingAddress(reinterpret_cast<uintptr_t>(&AppSetting));
	info.SetPluginUninstallAddress(reinterpret_cast<uintptr_t>(&AppUninstall));
	info.SetPluginDisabledAddress(reinterpret_cast<uintptr_t>(&AppDisabled));
	info.SetSmsCodeHandlerAddress(reinterpret_cast<uintptr_t>(&GetSMSVerificationCode));
	info.SetSliderHandlerAddress(reinterpret_cast<uintptr_t>(&SliderRecognition));

	info.RequestPermissionByName(xlz::kApiName_OutputLog_Utf8, "debug log");
	info.RequestPermissionByName(xlz::kApiName_SendGroupMessage_Utf8, "send group message");
	info.RequestPermissionByName(xlz::kApiName_GetPluginDataDirectory_Utf8, "plugin data directory");

	const std::string jsonUtf8 = info.BuildJsonUtf8();
	g_appInfoJsonAcp = Utf8ToAcpLossy(jsonUtf8);
	return g_appInfoJsonAcp.c_str();
}
