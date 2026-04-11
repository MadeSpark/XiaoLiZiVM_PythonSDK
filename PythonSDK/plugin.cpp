#define XLZSDK_EXPORTS

#include "../CppSDK/include/xlz_appinfo.h"
#include "../CppSDK/include/xlz_api_wrappers.h"
#include "../CppSDK/include/xlz_encoding.h"
#include "../CppSDK/include/xlz_event_utf8.h"
#include "../CppSDK/include/xlz_events.h"
#include "../CppSDK/include/xlz_exports.h"
#include "../CppSDK/include/xlz_sdk.h"
#include "../CppSDK/include/xlz_zh_names_utf8.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <mutex>

static bool g_debugEnabled = false; // 调试开关：默认关闭；需要时手动改为 true
static bool g_debugLogCleared = false;

static void DebugLog(const std::string& msg)
{
	if(!g_debugEnabled) return;
	if(!g_debugLogCleared){
		g_debugLogCleared = true;
		::DeleteFileW(L"debug.log"); // 首次调用时清除旧日志
	}
	std::ofstream f("debug.log", std::ios::app);
	if(!f) return;
	SYSTEMTIME st{}; ::GetLocalTime(&st);
	f << "[" << st.wYear << "-" << st.wMonth << "-" << st.wDay << " "
	  << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "." << st.wMilliseconds << "] "
	  << msg << "\n";
}

static void ForceLog(const std::string& msg)
{
	// 强制写日志，不受 g_debugEnabled 控制，用于恢复状态时的诊断
	std::ofstream f("debug.log", std::ios::app);
	if(!f) return;
	SYSTEMTIME st{}; ::GetLocalTime(&st);
	f << "[" << st.wYear << "-" << st.wMonth << "-" << st.wDay << " "
	  << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "." << st.wMilliseconds << "] "
	  << msg << "\n";
}

static xlz::Sdk g_sdk;
static std::string g_appInfoJsonAcp;
static std::string g_dllPathUtf8;
static std::string g_dllDirUtf8;
static std::string g_pyPayloadDirUtf8;
static std::string g_pyRuntimeDirUtf8;
static std::string g_pyModuleName;
static std::string g_lastUtf8Ret;
static std::vector<std::string> g_tempUsc2AnsiPool;
static std::mutex g_pyCallMutex;

using XLZ_PyInitialize_t = int(__stdcall*)();
using PyRun_SimpleString_t = int(*)(const char*);
using PyGILState_STATE = int;
using PyGILState_Ensure_t = PyGILState_STATE(*)();
using PyGILState_Release_t = void(*)(PyGILState_STATE);
using PyGILState_Check_t = int(*)();
using PyEval_SaveThread_t = void*(*)();
static XLZ_PyInitialize_t g_XLZ_PyInitialize = nullptr;
static PyRun_SimpleString_t g_PyRun_SimpleString = nullptr;
static PyGILState_Ensure_t g_PyGILState_Ensure = nullptr;
static PyGILState_Release_t g_PyGILState_Release = nullptr;
static PyGILState_Check_t g_PyGILState_Check = nullptr;
static PyEval_SaveThread_t g_PyEval_SaveThread = nullptr;

static std::string Utf8ToAcpLossy(const std::string& s)
{
	int wn = ::MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);
	if(wn<=0) return {};
	std::wstring w(wn,0); ::MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),wn);
	int bn = ::WideCharToMultiByte(CP_ACP,0,w.data(),wn,nullptr,0,nullptr,nullptr);
	if(bn<=0) return {};
	std::string o(bn,0); ::WideCharToMultiByte(CP_ACP,0,w.data(),wn,o.data(),bn,nullptr,nullptr);
	return o;
}

static std::string AcpToUtf8(const char* a)
{
	if(!a) return {};
	int wn=::MultiByteToWideChar(CP_ACP,0,a,-1,nullptr,0); if(wn<=1) return {};
	std::wstring w(wn,0); ::MultiByteToWideChar(CP_ACP,0,a,-1,w.data(),wn);
	int un=::WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,nullptr,0,nullptr,nullptr); if(un<=1) return {};
	std::string o(un-1,0); ::WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,o.data(),un,nullptr,nullptr);
	return o;
}

static std::wstring U8W(const std::string& s)
{
	int n=::MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0); if(n<=0) return {};
	std::wstring w(n,0); ::MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),w.data(),n); return w;
}

static std::string WU8(const wchar_t* w)
{
	if(!w) return {};
	int n=::WideCharToMultiByte(CP_UTF8,0,w,-1,nullptr,0,nullptr,nullptr); if(n<=1) return {};
	std::string o(n-1,0); ::WideCharToMultiByte(CP_UTF8,0,w,-1,o.data(),n,nullptr,nullptr); return o;
}

static std::string EscapePy(const std::string& s)
{
	std::string o; o.reserve(s.size()+8);
	for(char c:s){
		if(c=='\\'||c=='\'') o+='\\';
		if(c=='\n'){o+="\\n";continue;} if(c=='\r'){o+="\\r";continue;}
		o+=c;
	}
	return o;
}

static std::string EscapeFlatText(const std::string& s)
{
	std::string o; o.reserve(s.size()+8);
	for(char c:s){
		if(c=='\\') o+="\\\\";
		else if(c=='\n') o+="\\n";
		else if(c=='\r') o+="\\r";
		else o+=c;
	}
	return o;
}

static std::string UnescapeFlatText(const std::string& s)
{
	std::string o; o.reserve(s.size());
	for(size_t i=0;i<s.size();++i){
		const char c=s[i];
		if(c!='\\' || i+1>=s.size()){
			o+=c;
			continue;
		}
		const char n=s[i+1];
		if(n=='n'){ o+='\n'; ++i; }
		else if(n=='r'){ o+='\r'; ++i; }
		else if(n=='\\'){ o+='\\'; ++i; }
		else o+=c;
	}
	return o;
}

static void TrimRightAsciiWhitespace(std::string& s)
{
	while(!s.empty()){
		const unsigned char c = static_cast<unsigned char>(s.back());
		if(c==' ' || c=='\t' || c=='\r' || c=='\n') s.pop_back();
		else break;
	}
}

static void NormalizeReturnedUtf8(const char* api, std::string& s)
{
	TrimRightAsciiWhitespace(s);
	if(!api || s.empty()) return;
	if(std::strcmp(api, xlz::kApiName_GetPluginDataDirectory_Utf8)!=0) return;

	bool hasTrailingSlash = false;
	while(!s.empty() && (s.back()=='\\' || s.back()=='/')){
		hasTrailingSlash = true;
		s.pop_back();
	}
	TrimRightAsciiWhitespace(s);
	if(hasTrailingSlash) s.push_back('\\');
}

static bool ResolveSelfPath()
{
	wchar_t p[MAX_PATH]={}; HMODULE h=nullptr;
	if(!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,(LPCWSTR)&ResolveSelfPath,&h)) { DebugLog("ResolveSelfPath: GetModuleHandleExW failed"); return false; }
	if(!::GetModuleFileNameW(h,p,MAX_PATH)) { DebugLog("ResolveSelfPath: GetModuleFileNameW failed"); return false; }
	g_dllPathUtf8=WU8(p);
	auto pos=g_dllPathUtf8.find_last_of("\\/");
	g_dllDirUtf8=(pos==std::string::npos)?"":g_dllPathUtf8.substr(0,pos);
	DebugLog(std::string("ResolveSelfPath dll=")+g_dllPathUtf8);
	return !g_dllDirUtf8.empty();
}

static std::vector<std::string> Split(const std::string& s,char sep)
{
	std::vector<std::string> o; std::string c;
	for(char ch:s){if(ch==sep){o.push_back(c);c.clear();}else c+=ch;} o.push_back(c); return o;
}

static std::vector<uint32_t> BuildPackedArgs(const char* s)
{
	xlz::ArgPacker p;
	if(!s||!*s) return p.Data();
	for(const auto& item:Split(s,'|')){
		auto pos=item.find(':'); if(pos==std::string::npos) continue;
		const std::string t=item.substr(0,pos),v=item.substr(pos+1);
		if(t=="u32") p.PushU32((uint32_t)std::stoul(v));
		else if(t=="u64") p.PushU64((uint64_t)std::stoull(v));
		else if(t=="i32") p.PushU32((uint32_t)std::stol(v));
		else if(t=="i64") p.PushU64((uint64_t)std::stoll(v));
		else if(t=="b") p.PushU32((v=="1"||v=="true")?1u:0u);
		else if(t=="p") p.PushPtr((const void*)(uintptr_t)std::stoull(v));
		else if(t=="s"){g_tempUsc2AnsiPool.push_back(xlz::Utf8ToUsc2Ansi(v.c_str()));p.PushPtr(g_tempUsc2AnsiPool.back().c_str());}
	}
	return p.Data();
}

static std::vector<uint32_t> BuildPackedArgsWithPluginKey(const char* s)
{
	xlz::ArgPacker p;
	p.PushPtr(g_sdk.PluginKey().c_str());
	auto rest = BuildPackedArgs(s);
	auto out = p.Data();
	out.insert(out.end(), rest.begin(), rest.end());
	return out;
}

static std::string ReadAllText(const std::string& p)
{
	FILE* fp=nullptr;
	_wfopen_s(&fp, U8W(p).c_str(), L"rb");
	if(!fp) return {};
	std::string out;
	char buf[4096];
	for(;;){
		size_t n=fread(buf,1,sizeof(buf),fp);
		if(n>0) out.append(buf,n);
		if(n<sizeof(buf)) break;
	}
	fclose(fp);
	return out;
}

static std::string SanitizePathPart(std::string s)
{
	for(char& c:s){
		if(c=='<'||c=='>'||c==':'||c=='"'||c=='/'||c=='\\'||c=='|'||c=='?'||c=='*') c='_';
	}
	if(s.empty()) s="_";
	return s;
}

static bool EnsureDirRecursive(const std::string& pathUtf8)
{
	if(pathUtf8.empty()) return false;
	std::string cur;
	for(char ch: pathUtf8){
		cur.push_back(ch);
		if(ch=='\\' || ch=='/'){
			if(cur.size()<=3) continue; // skip "C:\"
			::CreateDirectoryW(U8W(cur).c_str(),nullptr);
		}
	}
	return ::CreateDirectoryW(U8W(pathUtf8).c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS;
}

// ===== Payload footer =====
static constexpr uint8_t kMagic[8]={'X','L','Z','P','Y','Z','I','P'};
struct PayloadFooter{uint32_t version;uint64_t offset;uint64_t size;uint32_t reserved;};

static bool RunPy(const std::string& code);

static bool ReadPayloadFooter(const std::string& dll,PayloadFooter& f)
{
	FILE* fp=nullptr;
	_wfopen_s(&fp, U8W(dll).c_str(), L"rb");
	if(!fp){ DebugLog("ReadPayloadFooter: open dll failed"); return false; }
	if(_fseeki64(fp,0,SEEK_END)!=0){ fclose(fp); DebugLog("ReadPayloadFooter: seek end failed"); return false; }
	const uint64_t sz=(uint64_t)_ftelli64(fp);
	DebugLog(std::string("ReadPayloadFooter size=")+std::to_string(sz));
	if(sz<32){ fclose(fp); return false; }
	if(_fseeki64(fp,(long long)(sz-32),SEEK_SET)!=0){ fclose(fp); DebugLog("ReadPayloadFooter: seek footer failed"); return false; }
	uint8_t mg[8]={0};
	if(fread(mg,1,8,fp)!=8){ fclose(fp); DebugLog("ReadPayloadFooter: read magic failed"); return false; }
	if(std::memcmp(mg,kMagic,8)!=0){ fclose(fp); DebugLog("ReadPayloadFooter: magic mismatch"); return false; }
	if(fread(&f.version,1,4,fp)!=4 || fread(&f.offset,1,8,fp)!=8 || fread(&f.size,1,8,fp)!=8 || fread(&f.reserved,1,4,fp)!=4){
		fclose(fp); DebugLog("ReadPayloadFooter: read body failed"); return false;
	}
	fclose(fp);
	DebugLog(std::string("ReadPayloadFooter ok version=")+std::to_string(f.version)+", offset="+std::to_string(f.offset)+", size="+std::to_string(f.size));
	return true;
}

static std::string PayloadHash(const std::string& dll,const PayloadFooter& f)
{
	FILE* fp=nullptr;
	_wfopen_s(&fp, U8W(dll).c_str(), L"rb");
	if(!fp) return "00000000";
	if(_fseeki64(fp,(long long)f.offset,SEEK_SET)!=0){ fclose(fp); return "00000000"; }
	uint32_t h=0x811c9dc5u;
	std::vector<char> buf(4096);
	uint64_t left=f.size;
	while(left>0){
		size_t need=(size_t)((left<buf.size())?left:buf.size());
		size_t n=fread(buf.data(),1,need,fp);
		if(n==0) break;
		for(size_t i=0;i<n;i++){ h^=(uint8_t)buf[i]; h*=0x01000193u; }
		left-=n;
	}
	fclose(fp);
	char out[12]; sprintf_s(out,"%08x",h); return out;
}

static void CleanupOldPayloadDirs(const std::string& pk,const std::string& keepHash)
{
	const std::string root="main\\work_python_plugin_tmp\\"+pk;
	std::ostringstream py;
	py << "import os,shutil\n";
	py << "_root=r'" << EscapePy(root) << "'\n";
	py << "_keep=r'" << EscapePy(keepHash) << "'\n";
	py << "if os.path.isdir(_root):\n";
	py << "    for _n in os.listdir(_root):\n";
	py << "        _p=os.path.join(_root,_n)\n";
	py << "        if _n!=_keep and os.path.isdir(_p):\n";
	py << "            try: shutil.rmtree(_p, ignore_errors=True)\n";
	py << "            except Exception: pass\n";
	RunPy(py.str());
}

static std::string HashString32Hex(const std::string& s)
{
	uint32_t h=0x811c9dc5u;
	for(unsigned char c: s){ h^=c; h*=0x01000193u; }
	char buf[12]; sprintf_s(buf, "%08x", h); return buf;
}

static std::string BuildSafePluginDirKey(const char* pluginkey)
{
	std::string raw = AcpToUtf8(pluginkey);
	if(raw.empty()) return "_";
	// 某些框架版本会把 apidata 误当作 pluginkey 传入，这里降级为 hash
	if(raw.size() > 64 || raw.find("_输出日志__") != std::string::npos || raw.find(",") != std::string::npos)
		return std::string("k_") + HashString32Hex(raw);
	raw = SanitizePathPart(raw);
	if(raw.size() > 48) raw.resize(48);
	return raw;
}

static bool RunPy(const std::string& code)
{
	if(!g_PyRun_SimpleString) { ForceLog("RunPy failed: PyRun_SimpleString null"); return false; }
	if(!g_PyGILState_Ensure || !g_PyGILState_Release){ ForceLog("RunPy failed: GIL funcs null"); return false; }
	if(g_PyGILState_Check && g_PyGILState_Check()){
		const int r = g_PyRun_SimpleString(code.c_str());
		const bool ok = (r==0);
		if(!ok) ForceLog(std::string("RunPy failed rc=")+std::to_string(r));
		return ok;
	}
	const PyGILState_STATE st = g_PyGILState_Ensure();
	const int r = g_PyRun_SimpleString(code.c_str());
	g_PyGILState_Release(st);
	const bool ok = (r==0);
	if(!ok) ForceLog(std::string("RunPy failed rc=")+std::to_string(r));
	return ok;
}

static bool ExtractPayload(const std::string& dll,const PayloadFooter& f,const std::string& dir)
{
	FILE* fp=nullptr;
	_wfopen_s(&fp, U8W(dll).c_str(), L"rb");
	if(!fp){ DebugLog("ExtractPayload: open dll failed"); return false; }
	if(_fseeki64(fp,(long long)f.offset,SEEK_SET)!=0){ fclose(fp); DebugLog("ExtractPayload: seek offset failed"); return false; }
	std::vector<char> buf((size_t)f.size);
	if(fread(buf.data(),1,(size_t)f.size,fp)!=(size_t)f.size){ fclose(fp); DebugLog("ExtractPayload: read payload failed"); return false; }
	fclose(fp);

	const std::string tmp=dir+"\\_tmp_.zip";
	FILE* ofp=nullptr;
	_wfopen_s(&ofp, U8W(tmp).c_str(), L"wb");
	if(!ofp){ DebugLog("ExtractPayload: open tmp zip failed"); return false; }
	if(fwrite(buf.data(),1,(size_t)f.size,ofp)!=(size_t)f.size){ fclose(ofp); DebugLog("ExtractPayload: write tmp zip failed"); return false; }
	fclose(ofp);

	std::ostringstream py;
	py<<"import zipfile,os\n";
	py<<"_z=zipfile.ZipFile(r'";
	py<<EscapePy(tmp)<<"')\n";
	py<<"_z.extractall(r'";
	py<<EscapePy(dir)<<"')\n";
	py<<"_z.close()\nos.remove(r'";
	py<<EscapePy(tmp)<<"')\n";
	return RunPy(py.str());
}

static bool InitPythonBridge()
{
	if(!ResolveSelfPath()) return false;
	HMODULE hRt=::LoadLibraryW(L"xlz_pyruntime.dll"); if(!hRt){ DebugLog("InitPythonBridge: load xlz_pyruntime.dll failed"); return false; }
	g_XLZ_PyInitialize=(XLZ_PyInitialize_t)::GetProcAddress(hRt,"XLZ_PyInitialize");
	int pyInitRet = 0;
	if(g_XLZ_PyInitialize){
		pyInitRet = g_XLZ_PyInitialize();
	}
	DebugLog(std::string("InitPythonBridge: XLZ_PyInitialize ret=")+std::to_string(pyInitRet));
	HMODULE hPy=::GetModuleHandleW(L"python310.dll");
	if(!hPy) hPy=::LoadLibraryW(L"python310.dll");
	if(!hPy){ DebugLog("InitPythonBridge: python310.dll not loaded"); return false; }
	wchar_t pyPathW[MAX_PATH] = {};
	if(::GetModuleFileNameW(hPy, pyPathW, MAX_PATH)){
		std::string pyPathUtf8 = WU8(pyPathW);
		auto p = pyPathUtf8.find_last_of("\\/");
		g_pyRuntimeDirUtf8 = (p==std::string::npos)?std::string():pyPathUtf8.substr(0,p);
		DebugLog(std::string("InitPythonBridge: python dir=")+g_pyRuntimeDirUtf8);
	}
	g_PyRun_SimpleString=(PyRun_SimpleString_t)::GetProcAddress(hPy,"PyRun_SimpleString");
	g_PyGILState_Ensure=(PyGILState_Ensure_t)::GetProcAddress(hPy,"PyGILState_Ensure");
	g_PyGILState_Release=(PyGILState_Release_t)::GetProcAddress(hPy,"PyGILState_Release");
	g_PyGILState_Check=(PyGILState_Check_t)::GetProcAddress(hPy,"PyGILState_Check");
	g_PyEval_SaveThread=(PyEval_SaveThread_t)::GetProcAddress(hPy,"PyEval_SaveThread");
	const bool ok = g_PyRun_SimpleString!=nullptr && g_PyGILState_Ensure!=nullptr && g_PyGILState_Release!=nullptr;
	// 只在“当前线程确实持有GIL”时才允许 SaveThread，避免多插件重复加载时误调用导致初始化失败
	if(ok && g_PyEval_SaveThread && g_PyGILState_Check && pyInitRet > 0 && g_PyGILState_Check()){
		g_PyEval_SaveThread();
	}
	DebugLog(std::string("InitPythonBridge done ok=") + (ok?"1":"0"));
	return ok;
}

static bool SetupPayload(const char* pluginkey)
{
	const std::string pk=BuildSafePluginDirKey(pluginkey);
	DebugLog(std::string("SetupPayload pluginkey_key=")+pk);
	PayloadFooter footer;
	if(!ReadPayloadFooter(g_dllPathUtf8,footer)){
		g_pyPayloadDirUtf8=g_dllDirUtf8+"\\py_payload";
		g_pyModuleName = "xlz_plugin_main_" + pk + "_fallback";
		DebugLog(std::string("SetupPayload no footer, fallback dir=")+g_pyPayloadDirUtf8);
		return true;
	}
	const std::string hash=PayloadHash(g_dllPathUtf8,footer);
	g_pyModuleName = "xlz_plugin_main_" + pk + "_" + hash;
	const std::string tmpDir="main\\work_python_plugin_tmp\\"+pk+"\\"+hash;
	const std::string done=tmpDir+"\\.xlz_ok";
	DebugLog(std::string("SetupPayload target dir=")+tmpDir);
	if(::GetFileAttributesW(U8W(done).c_str())!=INVALID_FILE_ATTRIBUTES){
		g_pyPayloadDirUtf8=tmpDir; DebugLog("SetupPayload already extracted");
		CleanupOldPayloadDirs(pk, hash);
		return true;
	}
	if(!EnsureDirRecursive(tmpDir)){ DebugLog("SetupPayload EnsureDirRecursive failed"); return false; }
	if(!ExtractPayload(g_dllPathUtf8,footer,tmpDir)){ DebugLog("SetupPayload ExtractPayload failed"); return false; }
	{std::ofstream f(done);f<<"ok";}
	g_pyPayloadDirUtf8=tmpDir;
	CleanupOldPayloadDirs(pk, hash);
	DebugLog(std::string("SetupPayload success dir=")+g_pyPayloadDirUtf8);
	return true;
}

// ===== Python 桥接导出 =====
XLZ_API void XLZ_CALL XLZ_Bridge_ResetTempStrings(){g_tempUsc2AnsiPool.clear();}

XLZ_API const char* XLZ_CALL XLZ_Bridge_Utf8ToUsc2AnsiPtr(const char* u)
{
	g_tempUsc2AnsiPool.push_back(xlz::Utf8ToUsc2Ansi(u?u:""));
	return g_tempUsc2AnsiPool.back().c_str();
}

XLZ_API void XLZ_CALL XLZ_Bridge_OutputLog(const char* m)
{
	xlz::OutputLog(g_sdk,m?m:"");
}

XLZ_API const char* XLZ_CALL XLZ_Bridge_SendGroupMessage(long long tqq,long long gqq,const char* msg,int anon)
{
	g_lastUtf8Ret=xlz::SendGroupMessage(g_sdk,(int64_t)tqq,(int64_t)gqq,msg?msg:"",anon!=0);
	return g_lastUtf8Ret.c_str();
}

XLZ_API const char* XLZ_CALL XLZ_Bridge_CallApiReturnUtf8(const char* api,const char* args)
{
	DebugLog(std::string("XLZ_Bridge_CallApiReturnUtf8: api=")+(api?api:"null")+", args="+(args?args:"null"));
	g_lastUtf8Ret=g_sdk.CallApiReturnUtf8(api?api:"",BuildPackedArgsWithPluginKey(args));
	NormalizeReturnedUtf8(api, g_lastUtf8Ret);
	DebugLog(std::string("XLZ_Bridge_CallApiReturnUtf8: ret=")+g_lastUtf8Ret);
	return g_lastUtf8Ret.c_str();
}

XLZ_API unsigned int XLZ_CALL XLZ_Bridge_CallApiReturnU32(const char* api,const char* args)
{
	return g_sdk.CallApiReturnU32(api?api:"",BuildPackedArgsWithPluginKey(args));
}

XLZ_API void XLZ_CALL XLZ_Bridge_CallApiVoid(const char* api,const char* args)
{
	g_sdk.CallApiVoid(api?api:"",BuildPackedArgsWithPluginKey(args));
}

// 专用图片上传接口：在C++层持有图片数据，避免Python GC问题
XLZ_API const char* XLZ_CALL XLZ_Bridge_UploadGroupImage(long long thisQq, long long groupQq, int isFlash, const void* picData, int picSize)
{
	DebugLog(std::string("XLZ_Bridge_UploadGroupImage: thisQq=")+std::to_string(thisQq)+", groupQq="+std::to_string(groupQq)+", size="+std::to_string(picSize));
	// 在C++层复制一份数据，确保框架读取时数据有效
	std::vector<char> buf(static_cast<const char*>(picData), static_cast<const char*>(picData)+picSize);
	g_lastUtf8Ret = xlz::UploadGroupImage(g_sdk, thisQq, groupQq, isFlash!=0, buf.data(), (uint32_t)picSize);
	DebugLog(std::string("XLZ_Bridge_UploadGroupImage: ret=")+g_lastUtf8Ret);
	return g_lastUtf8Ret.c_str();
}

// ===== 事件转发（空壳，全部转 Python）=====
static bool IsNoArgCallback(const std::string& func)
{
	return func=="on_enable"
		|| func=="on_disable"
		|| func=="on_uninstall"
		|| func=="on_setting";
}

static int PyCallEventWithReturn(const std::string& func, const std::string& dictLiteral)
{
	if(g_pyPayloadDirUtf8.empty() || g_pyModuleName.empty()){
		ForceLog(std::string("PyCallEventWithReturn: not ready, func=")+func);
		// 用当前DLL自身路径定位状态文件，不依赖可能为空的全局变量
		wchar_t selfW[MAX_PATH]={};
		HMODULE hSelf=nullptr;
		::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)&PyCallEventWithReturn,&hSelf);
		if(hSelf) ::GetModuleFileNameW(hSelf,selfW,MAX_PATH);
		const std::string selfPath = WU8(selfW);
		const std::string selfDir = selfPath.substr(0, selfPath.find_last_of("\\/"));
		ForceLog(std::string("PyCallEventWithReturn: self dll=")+selfPath);
		// 扫描 selfDir 下所有 .xlz_pystate_*.txt 文件
		std::string foundState;
		{
			const std::wstring pat = U8W(selfDir) + L"\\.xlz_pystate_*.txt";
			WIN32_FIND_DATAW fd{};
			HANDLE h=::FindFirstFileW(pat.c_str(),&fd);
			if(h!=INVALID_HANDLE_VALUE){
				foundState = selfDir + "\\" + WU8(fd.cFileName);
				::FindClose(h);
			}
		}
		ForceLog(std::string("PyCallEventWithReturn: state file=")+foundState);
		if(!foundState.empty()){
			std::ifstream sf(U8W(foundState).c_str());
			if(sf){
				std::string payloadDir, moduleName, apiData, pluginKey;
				std::getline(sf, payloadDir);
				std::getline(sf, moduleName);
				std::getline(sf, apiData);
				std::getline(sf, pluginKey);
				ForceLog(std::string("PyCallEventWithReturn: state payloadDir=")+payloadDir+", module="+moduleName);
				if(!payloadDir.empty() && !moduleName.empty()){
					g_pyPayloadDirUtf8 = payloadDir;
					g_pyModuleName = moduleName;
					g_dllPathUtf8 = selfPath;
					g_dllDirUtf8 = selfDir;
					if(!apiData.empty()) g_sdk.Initialize(apiData.c_str(), pluginKey.c_str());
					if(!InitPythonBridge()) { ForceLog("PyCallEventWithReturn: restore InitPythonBridge failed"); return 0; }
					ForceLog("PyCallEventWithReturn: restored state from file ok");
				}
			} else {
				ForceLog("PyCallEventWithReturn: state file open failed");
			}
		}
		if(g_pyPayloadDirUtf8.empty() || g_pyModuleName.empty()){
			ForceLog("PyCallEventWithReturn: still not ready, giving up");
			return 0;
		}
	}
	std::lock_guard<std::mutex> lock(g_pyCallMutex);
	ForceLog(std::string("PyCallEventWithReturn: executing func=")+func+", PyRun="+(g_PyRun_SimpleString?"ok":"NULL"));
	const std::string retFile = g_pyPayloadDirUtf8 + "\\_xlz_ret_.txt";
	std::ostringstream py;
	py << "import sys, os, importlib.util\n";
	py << "def _xlz_call_" << EscapePy(g_pyModuleName) << "():\n";
	py << "    _base=r'" << EscapePy(g_pyPayloadDirUtf8) << "'\n";
	py << "    _dll_dir=r'" << EscapePy(g_dllDirUtf8) << "'\n";
	py << "    _py_dir=r'" << EscapePy(g_pyRuntimeDirUtf8) << "'\n";
	py << "    _py_dlls=_py_dir + r'\\DLLs'\n";
	py << "    for _p in (_base, _base + r'\\py_payload', _dll_dir, _py_dir, _py_dlls):\n";
	py << "        if _p and _p not in sys.path: sys.path.insert(0,_p)\n";
	py << "    _mod=r'" << EscapePy(g_pyModuleName) << "'\n";
	py << "    _pm_path=os.path.join(_base,'plugin_main.py')\n";
	py << "    if not os.path.isfile(_pm_path): _pm_path=os.path.join(_base,'py_payload','plugin_main.py')\n";
	py << "    if _mod in sys.modules:\n";
	py << "        _pm=sys.modules[_mod]\n";
	py << "    else:\n";
	py << "        _sp=importlib.util.spec_from_file_location(_mod,_pm_path)\n";
	py << "        _pm=importlib.util.module_from_spec(_sp)\n";
	py << "        sys.modules[_mod]=_pm\n";
	py << "        _sp.loader.exec_module(_pm)\n";
	py << "    _ret=0\n";
	py << "    try:\n";
	if(IsNoArgCallback(func)){
		py << "        _ret=getattr(_pm,'" << func << "',lambda:0)()\n";
	}else{
		py << "        _ret=getattr(_pm,'" << func << "',lambda e:0)(" << dictLiteral << ")\n";
	}
	py << "        _ret=int(_ret) if _ret is not None else 0\n";
	py << "    except Exception:\n";
	py << "        import traceback\n";
	py << "        open('debug.log','a',encoding='utf-8').write('[event] '+traceback.format_exc()+'\\n')\n";
	py << "    open(r'" << EscapePy(retFile) << "','w').write(str(_ret))\n";
	py << "    return _ret\n";
	py << "_xlz_call_" << EscapePy(g_pyModuleName) << "()\n";
	if(!RunPy(py.str())) return 0;
	std::string ret_str;
	try{
		std::ifstream f(U8W(retFile).c_str());
		if(f) std::getline(f, ret_str);
	}catch(...){}
	::DeleteFileW(U8W(retFile).c_str());
	return ret_str.empty() ? 0 : std::stoi(ret_str);
}

static void PyCallEvent(const std::string& func, const std::string& dictLiteral)
{
	PyCallEventWithReturn(func, dictLiteral);
}

XLZ_API int XLZ_CALL RecviceGroupMesg(void* data)
{
	const auto* ev=reinterpret_cast<const xlz::GroupMessageEvent*>(data);
	if(!ev){ DebugLog("RecviceGroupMesg: null data"); return 0; }
	const std::string msg = xlz::GetGroupMessageContentUtf8(*ev);
	DebugLog(std::string("RecviceGroupMesg: group=")+std::to_string((long long)ev->MessageGroupQQ)+", sender="+std::to_string((long long)ev->SenderQQ)+", msg="+msg);
	DebugLog("RecviceGroupMesg: before PyCallEvent");
	std::ostringstream d;
	d << "{'this_qq':" << ev->ThisQQ
	  << ",'group_qq':" << ev->MessageGroupQQ
	  << ",'sender_qq':" << ev->SenderQQ
	  << ",'message_req':" << ev->MessageReq
	  << ",'message_receive_time':" << ev->MessageReceiveTime
	  << ",'message_send_time':" << ev->MessageSendTime
	  << ",'message_random':" << ev->MessageRandom
	  << ",'message_clip':" << ev->MessageClip
	  << ",'message_clip_count':" << ev->MessageClipCount
	  << ",'message_clip_id':" << ev->MessageClipID
	  << ",'message_type':" << ev->MessageType
	  << ",'bubble_id':" << ev->BubbleID
	  << ",'group_chat_level':" << ev->GroupChatLevel
	  << ",'pendant_id':" << ev->PendantID
	  << ",'anonymous_id':" << ev->AnonymousId
	  << ",'font_id':" << ev->FontId
	  << ",'message':'" << EscapePy(msg) << "'";
	d << ",'group_name':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->SourceGroupName?ev->SourceGroupName:"")) << "'";
	d << ",'sender_nick':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->SenderNickname?ev->SenderNickname:"")) << "'";
	d << ",'sender_title':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->SenderTitle?ev->SenderTitle:"")) << "'";
	d << ",'reply_message':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->ReplyMessageContent?ev->ReplyMessageContent:"")) << "'";
	d << ",'anonymous_nickname':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->AnonymousNickname?ev->AnonymousNickname:"")) << "'";
	d << ",'reserved_parameters':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->ReservedParameters?ev->ReservedParameters:"")) << "'";
	d << "}";
	return PyCallEventWithReturn("on_group_message",d.str());
}

XLZ_API int XLZ_CALL RecvicePrivateMsg(void* data)
{
	const auto* ev=reinterpret_cast<const xlz::PrivateMessageEvent*>(data);
	if(!ev){ DebugLog("RecvicePrivateMsg: null data"); return 0; }
	DebugLog(std::string("RecvicePrivateMsg: sender=")+std::to_string((long long)ev->SenderQQ));
	std::ostringstream d;
	d << "{'this_qq':" << ev->ThisQQ
	  << ",'sender_qq':" << ev->SenderQQ
	  << ",'message_req':" << ev->MessageReq
	  << ",'message_seq':" << ev->MessageSeq
	  << ",'message_receive_time':" << ev->MessageReceiveTime
	  << ",'message_group_qq':" << ev->MessageGroupQQ
	  << ",'message_send_time':" << ev->MessageSendTime
	  << ",'message_random':" << ev->MessageRandom
	  << ",'message_clip':" << ev->MessageClip
	  << ",'message_clip_count':" << ev->MessageClipCount
	  << ",'message_clip_id':" << ev->MessageClipID
	  << ",'bubble_id':" << ev->BubbleID
	  << ",'message_type':" << ev->MessageType
	  << ",'message_sub_type':" << ev->MessageSubType
	  << ",'message_sub_temporary_type':" << ev->MessageSubTemporaryType
	  << ",'red_envelope_type':" << ev->RedEnvelopeType
	  << ",'source_event_qq':" << ev->SourceEventQQ
	  << ",'msg_group_id':" << ev->MsgGroupId
	  << ",'message':'" << EscapePy(xlz::GetPrivateMessageContentUtf8(*ev)) << "'";
	d << ",'source_event_qq_name':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->SourceEventQQName?ev->SourceEventQQName:"")) << "'";
	d << ",'file_id':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->FileID?ev->FileID:"")) << "'";
	d << ",'file_md5':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->FileMD5?ev->FileMD5:"")) << "'";
	d << ",'file_name':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->FileName?ev->FileName:"")) << "'";
	d << "}";
	return PyCallEventWithReturn("on_private_message",d.str());
}

XLZ_API int XLZ_CALL RecviceEventCallBack(void* data)
{
	const auto* ev=reinterpret_cast<const xlz::EventTypeBase*>(data);
	if(!ev) return 0;
	std::ostringstream d;
	d << "{'this_qq':" << ev->ThisQQ
	  << ",'event_type':" << ev->EventType
	  << ",'event_sub_type':" << ev->EventSubType
	  << ",'trigger_qq':" << ev->TriggerQQ
	  << ",'source_group_qq':" << ev->SourceGroupQQ
	  << ",'operate_qq':" << ev->OperateQQ
	  << ",'message_seq':" << ev->MessageSeq
	  << ",'message_time':" << ev->MessageTime
	  << ",'message':'" << EscapePy(xlz::GetEventMessageContentUtf8(*ev)) << "'";
	d << ",'source_group_name':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->SourceGroupName?ev->SourceGroupName:"")) << "'";
	d << ",'operate_qq_name':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->OperateQQName?ev->OperateQQName:"")) << "'";
	d << ",'trigger_qq_name':'" << EscapePy(xlz::Usc2AnsiToUtf8(ev->TriggerQQName?ev->TriggerQQName:"")) << "'";
	d << "}";
	return PyCallEventWithReturn("on_event_message",d.str());
}

XLZ_API int XLZ_CALL RotbotAppEnable(){return PyCallEventWithReturn("on_enable","{}");}
XLZ_API int XLZ_CALL AppSetting(){PyCallEvent("on_setting","{}");return 0;}
XLZ_API void XLZ_CALL AppUninstall(){
	PyCallEvent("on_uninstall","{}");
	std::lock_guard<std::mutex> lock(g_pyCallMutex);
	if(!g_pyModuleName.empty() && g_PyRun_SimpleString){
		std::ostringstream py;
		py << "import sys\n";
		py << "_mod=r'" << EscapePy(g_pyModuleName) << "'\n";
		py << "if _mod in sys.modules:\n";
		py << "    del sys.modules[_mod]\n";
		RunPy(py.str());
	}
	g_pyModuleName.clear();
	g_pyPayloadDirUtf8.clear();
}
XLZ_API void XLZ_CALL AppDisabled(){PyCallEvent("on_disable","{}");}
XLZ_API void XLZ_CALL GetSMSVerificationCode(long long qq,void* phone){(void)qq;(void)phone;}
XLZ_API void XLZ_CALL SliderRecognition(long long qq,void* url){(void)qq;(void)url;}

XLZ_API const char* XLZ_CALL apprun(const char* pluginkey,const char* apidata)
{
	DebugLog("===== apprun enter =====");

	auto looksLikeApiData = [](const char* s)->bool{
		if(!s) return false;
		if(std::strchr(s,'{')==nullptr || std::strchr(s,':')==nullptr) return false;
		if(std::strstr(s,"output")!=nullptr) return false;
		if(std::strstr(s,"_输出日志__")!=nullptr) return true;
		return true;
	};
	const char* apiDataJson=nullptr;
	const char* pluginKeyRaw=nullptr;
	const size_t len1=pluginkey?std::strlen(pluginkey):0;
	const size_t len2=apidata?std::strlen(apidata):0;
	if(!looksLikeApiData(pluginkey) && looksLikeApiData(apidata)) { pluginKeyRaw=pluginkey; apiDataJson=apidata; }
	else if(looksLikeApiData(pluginkey) && !looksLikeApiData(apidata)) { pluginKeyRaw=apidata; apiDataJson=pluginkey; }
	else if(len2>=len1) { pluginKeyRaw=pluginkey; apiDataJson=apidata; }
	else { pluginKeyRaw=apidata; apiDataJson=pluginkey; }

	DebugLog(std::string("apprun normalized key len=")+std::to_string(pluginKeyRaw?std::strlen(pluginKeyRaw):0)+", api len="+std::to_string(apiDataJson?std::strlen(apiDataJson):0));

	g_sdk.Initialize(apiDataJson,pluginKeyRaw);
	if(!InitPythonBridge()){
		DebugLog("apprun: InitPythonBridge failed");
		if(g_debugEnabled) xlz::OutputLog(g_sdk,"[PythonCompat] Python bridge init failed");
		return "{}";
	}
	if(!SetupPayload(pluginKeyRaw)){
		DebugLog("apprun: SetupPayload failed");
		if(g_debugEnabled) xlz::OutputLog(g_sdk,"[PythonCompat] Payload setup failed");
		return "{}";
	}

	const std::string infoFile=g_pyPayloadDirUtf8+"\\.xlz_appinfo.txt";
	DebugLog(std::string("apprun payload dir=")+g_pyPayloadDirUtf8);
	DebugLog(std::string("apprun info file=")+infoFile);
	DebugLog(std::string("apprun module name=")+g_pyModuleName);
	DebugLog(std::string("apprun dll path=")+g_dllPathUtf8);
	std::ostringstream py;
	py << "import sys,traceback\n";
	py << "_base=r'" << EscapePy(g_pyPayloadDirUtf8) << "'\n";
	py << "_dll_dir=r'" << EscapePy(g_dllDirUtf8) << "'\n";
	py << "_py_dir=r'" << EscapePy(g_pyRuntimeDirUtf8) << "'\n";
	py << "_py_dlls=_py_dir + r'\\DLLs'\n";
	py << "for _p in (_base, _base + r'\\py_payload', _dll_dir, _py_dir, _py_dlls):\n";
	py << "    if _p and _p not in sys.path: sys.path.insert(0,_p)\n";
	py << "import os\n";
	py << "import importlib.util\n";
	py << "if 'xlz_sdk' in sys.modules: del sys.modules['xlz_sdk']\n";
	py << "_mod=r'" << EscapePy(g_pyModuleName) << "'\n";
	py << "if _mod in sys.modules: del sys.modules[_mod]\n";
	py << "# 清除所有旧的同 pluginkey 模块缓存，防止积累\n";
	py << "_pfx='xlz_plugin_main_" << EscapePy(BuildSafePluginDirKey(pluginKeyRaw)) << "_'\n";
	py << "_removed=[_k for _k in list(sys.modules.keys()) if _k.startswith(_pfx)]\n";
	py << "for _k in _removed: del sys.modules[_k]\n";
	py << "open('debug.log','a',encoding='utf-8').write('[apprun] cleared old module cache: '+str(_removed)+'\\n') if _removed else None\n";
	py << "os.environ['PATH'] = _dll_dir + ';' + _py_dir + ';' + _py_dlls + ';' + os.environ.get('PATH','')\n";
	py << "try:\n";
	py << "    os.add_dll_directory(_dll_dir)\n";
	py << "    os.add_dll_directory(_py_dir)\n";
	py << "    os.add_dll_directory(_py_dlls)\n";
	py << "except Exception:\n";
	py << "    pass\n";
	py << "try:\n";
	py << "    _mod=r'" << EscapePy(g_pyModuleName) << "'\n";
	py << "    _pm_path=os.path.join(_base,'plugin_main.py')\n";
	py << "    if not os.path.isfile(_pm_path): _pm_path=os.path.join(_base,'py_payload','plugin_main.py')\n";
	py << "    if _mod in sys.modules:\n";
	py << "        _pm=sys.modules[_mod]\n";
	py << "    else:\n";
	py << "        _sp=importlib.util.spec_from_file_location(_mod,_pm_path)\n";
	py << "        _pm=importlib.util.module_from_spec(_sp)\n";
	py << "        sys.modules[_mod]=_pm\n";
	py << "        _sp.loader.exec_module(_pm)\n";
	py << "    _ctx={'pluginkey':'" << EscapePy(AcpToUtf8(pluginKeyRaw)) << "','dll_path':r'" << EscapePy(g_dllPathUtf8) << "'}\n";
	py << "    _info=_pm.apprun(_ctx)\n";
	py << "    def _flat(_v):\n";
	py << "        return str(_v or '').replace('\\\\','\\\\\\\\').replace('\\r','\\\\r').replace('\\n','\\\\n')\n";
	py << "    open(r'" << EscapePy(infoFile) << "','w',encoding='utf-8').write('\\n'.join([\n";
	py << "f\"app_name={_flat(_info.get('app_name',''))}\",\n";
	py << "f\"author={_flat(_info.get('author',''))}\",\n";
	py << "f\"app_version={_flat(_info.get('app_version',''))}\",\n";
	py << "f\"description={_flat(_info.get('description',''))}\",\n";
	py << "f\"permissions={','.join(_info.get('permissions') or [])}\"\n";
	py << "]))\n";
	py << "except Exception:\n";
	py << "    open('debug.log','a',encoding='utf-8').write('[python] '+traceback.format_exc()+'\\n')\n";
	py << "    raise\n";
	if(!RunPy(py.str())){
		DebugLog("apprun: RunPy plugin_main.apprun failed");
		if(g_debugEnabled) xlz::OutputLog(g_sdk,"[PythonCompat] plugin_main.apprun execute failed");
		return "{}";
	}

	std::string name,author,version,desc,perms;
	const std::string txt=ReadAllText(infoFile);
	DebugLog(std::string("apprun info file bytes=")+std::to_string(txt.size()));
	if(!txt.empty()){
		std::istringstream in(txt);
		for(std::string line;std::getline(in,line);){
			auto p=line.find('='); if(p==std::string::npos) continue;
			auto k=line.substr(0,p),v=line.substr(p+1);
			if(k=="app_name") name=v;
			else if(k=="author") author=v;
			else if(k=="app_version") version=v;
			else if(k=="description") desc=UnescapeFlatText(v);
			else if(k=="permissions") perms=v;
		}
	}

	if(name.empty() || author.empty() || version.empty()){
		DebugLog(std::string("apprun missing fields name=")+name+", author="+author+", version="+version);
		if(g_debugEnabled) xlz::OutputLog(g_sdk,"[PythonCompat] app_name/author/app_version is empty");
		return "{}";
	}

	xlz::AppInfoBuilder info;
	info.SetAppName(name.c_str());
	info.SetAuthor(author.c_str());
	info.SetAppVersion(version.c_str());
	info.SetDescription(desc.c_str());
	info.SetPluginEnabledAddress((uintptr_t)&RotbotAppEnable);
	info.SetPrivateMessageAddress((uintptr_t)&RecvicePrivateMsg);
	info.SetGroupMessageAddress((uintptr_t)&RecviceGroupMesg);
	info.SetEventMessageAddress((uintptr_t)&RecviceEventCallBack);
	info.SetPluginSettingAddress((uintptr_t)&AppSetting);
	info.SetPluginUninstallAddress((uintptr_t)&AppUninstall);
	info.SetPluginDisabledAddress((uintptr_t)&AppDisabled);
	info.SetSmsCodeHandlerAddress((uintptr_t)&GetSMSVerificationCode);
	info.SetSliderHandlerAddress((uintptr_t)&SliderRecognition);
	info.RequestPermissionByName(xlz::kApiName_OutputLog_Utf8,"debug log");
	info.RequestPermissionByName(xlz::kApiName_SendGroupMessage_Utf8,"send group message");
	info.RequestPermissionByName(xlz::kApiName_UploadGroupImage_Utf8,"upload group image");

	// 根据 Python 返回的 permissions 动态申请权限
	if(!perms.empty()){
		// 建立中文权限名到 kApiName 的映射
		static const struct { const char* zh; const char* api; } kPermMap[] = {
			{"\xE8\xBE\x93\xE5\x87\xBA\xE6\x97\xA5\xE5\xBF\x97", xlz::kApiName_OutputLog_Utf8},
			{"\xE5\x8F\x91\xE9\x80\x81\xE5\xA5\xBD\xE5\x8F\x8B\xE6\xB6\x88\xE6\x81\xAF", xlz::kApiName_SendFriendMessage_Utf8},
			{"\xE5\x8F\x91\xE9\x80\x81\xE7\xBE\xA4\xE6\xB6\x88\xE6\x81\xAF", xlz::kApiName_SendGroupMessage_Utf8},
			{"\xE5\x8F\x96\xE6\xA1\x86\xE6\x9E\xB6\x51\x51", xlz::kApiName_GetFrameworkQQ_Utf8},
			{"\xE5\x8F\x96\xE7\xBE\xA4\xE5\x88\x97\xE8\xA1\xA8", xlz::kApiName_GetGroupList_Utf8},
			{"\xE5\x8F\x96\xE7\xBE\xA4\xE6\x88\x90\xE5\x91\x98\xE5\x88\x97\xE8\xA1\xA8", xlz::kApiName_GetGroupMemberList_Utf8},
			{"\xE5\x8F\x91\xE9\x80\x81\xE7\xBE\xA4\xE4\xB8\xB4\xE6\x97\xB6\xE6\xB6\x88\xE6\x81\xAF", xlz::kApiName_SendGroupTemporaryMessage_Utf8},
			{"\xE5\x8F\x91\xE9\x80\x81\xE7\xBE\xA4json\xE6\xB6\x88\xE6\x81\xAF", xlz::kApiName_SendGroupJsonMessage_Utf8},
			{"\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA5\xBD\xE5\x8F\x8B\xE5\x9B\xBE\xE7\x89\x87", xlz::kApiName_UploadFriendImage_Utf8},
			{"\xE4\xB8\x8A\xE4\xBC\xA0\xE7\xBE\xA4\xE5\x9B\xBE\xE7\x89\x87", xlz::kApiName_UploadGroupImage_Utf8},
			{"\xE4\xB8\x8A\xE4\xBC\xA0\xE5\xA5\xBD\xE5\x8F\x8B\xE8\xAF\xAD\xE9\x9F\xB3", xlz::kApiName_UploadFriendAudio_Utf8},
			{"\xE4\xB8\x8A\xE4\xBC\xA0\xE7\xBE\xA4\xE8\xAF\xAD\xE9\x9F\xB3", xlz::kApiName_UploadGroupAudio_Utf8},
			{"\xE4\xB8\x8A\xE4\xBC\xA0\xE7\xBE\xA4\xE6\x96\x87\xE4\xBB\xB6", xlz::kApiName_UploadGroupFile_Utf8},
			{"\xE7\xBE\xA4\xE8\x81\x8A\xE6\x89\x93\xE5\x8D\xA1", xlz::kApiName_GroupCheckIn_Utf8},
			{"\xE5\x88\x86\xE4\xBA\xAB\xE9\x9F\xB3\xE4\xB9\x90", xlz::kApiName_ShareMusic_Utf8},
			{"\xE5\x8F\x96\xE5\xA5\xBD\xE5\x8F\x8B\xE6\x96\x87\xE4\xBB\xB6\xE4\xB8\x8B\xE8\xBD\xBD\xE5\x9C\xB0\xE5\x9D\x80", xlz::kApiName_GetFriendFileDownloadUrl_Utf8},
			{"\xE6\x92\xA4\xE5\x9B\x9E\xE6\xB6\x88\xE6\x81\xAF_\xE7\xBE\xA4\xE8\x81\x8A", xlz::kApiName_RecallGroupMessage_Utf8},
			{"\xE7\xA6\x81\xE8\xA8\x80\xE7\xBE\xA4\xE6\x88\x90\xE5\x91\x98", xlz::kApiName_MuteGroupMember_Utf8},
			{"\xE5\x88\xA0\xE9\x99\xA4\xE7\xBE\xA4\xE6\x88\x90\xE5\x91\x98", xlz::kApiName_RemoveGroupMember_Utf8},
			{"\xE5\xA4\x84\xE7\x90\x86\xE5\xA5\xBD\xE5\x8F\x8B\xE9\xAA\x8C\xE8\xAF\x81\xE4\xBA\x8B\xE4\xBB\xB6", xlz::kApiName_HandleFriendVerificationEvent_Utf8},
			{"\xE5\xA4\x84\xE7\x90\x86\xE7\xBE\xA4\xE9\xAA\x8C\xE8\xAF\x81\xE4\xBA\x8B\xE4\xBB\xB6", xlz::kApiName_HandleGroupVerificationEvent_Utf8},
			{"\xE5\x8F\x96\xE7\xAE\xA1\xE7\x90\x86\xE5\xB1\x82\xE5\x88\x97\xE8\xA1\xA8", xlz::kApiName_GetAdministratorList_Utf8},
			{"\xE5\x8F\x96\xE5\x9B\xBE\xE7\x89\x87\xE4\xB8\x8B\xE8\xBD\xBD\xE5\x9C\xB0\xE5\x9D\x80", xlz::kApiName_GetImageDownloadUrl_Utf8},
			{"\xE5\xBC\xBA\xE5\x88\xB6\xE5\x8F\x96\xE6\x98\xB5\xE7\xA7\xB0", xlz::kApiName_GetNicknameForce_Utf8},
			{"\xE5\x8F\x96\xE6\x98\xB5\xE7\xA7\xB0_\xE4\xBB\x8E\xE7\xBC\x93\xE5\xAD\x98", xlz::kApiName_GetNicknameFromCache_Utf8},
			{"\xE5\x85\xA8\xE5\x91\x98\xE7\xA6\x81\xE8\xA8\x80", xlz::kApiName_MuteAll_Utf8},
			{"QQ\xE7\x82\xB9\xE8\xB5\x9E", xlz::kApiName_QQLike_Utf8},
			{"\xE5\x8F\x96\xE7\xBE\xA4\xE5\x90\x8D\xE7\x89\x87", xlz::kApiName_GetGroupCard_Utf8},
			{"\xE8\xAE\xBE\xE7\xBD\xAE\xE7\xBE\xA4\xE5\x90\x8D\xE7\x89\x87", xlz::kApiName_SetGroupCard_Utf8},
			{"\xE6\x8F\x90\xE5\x8F\x96\xE5\x9B\xBE\xE7\x89\x87\xE6\x96\x87\xE5\xAD\x97", xlz::kApiName_ExtractTextFromImage_Utf8},
			{"\xE8\xB0\x83\xE7\x94\xA8\xE6\x8C\x87\xE5\xAE\x9AOneBotInterface", xlz::kApiName_CallOneBotInterface_Utf8},
			{"\xE5\x8F\x96\xE7\xBE\xA4\xE6\x88\x90\xE5\x91\x98\xE4\xBF\xA1\xE6\x81\xAF", xlz::kApiName_GetGroupMemberInfo_Utf8},
			{"\xE5\x8F\x96\xE6\x8F\x92\xE4\xBB\xB6\xE6\x95\xB0\xE6\x8D\xAE\xE7\x9B\xAE\xE5\xBD\x95", xlz::kApiName_GetPluginDataDirectory_Utf8},
			{"\xE9\x87\x8D\xE8\xBD\xBD\xE8\x87\xAA\xE8\xBA\xAB", xlz::kApiName_ReloadItSelf_Utf8},
			{nullptr, nullptr}
		};
		for(const auto& perm : Split(perms, ',')){
			const std::string p = [](std::string s){
				while(!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
				while(!s.empty() && (s.back()==' '||s.back()=='\t')) s.pop_back();
				return s;
			}(perm);
			if(p.empty()) continue;
			bool found = false;
			for(int i = 0; kPermMap[i].zh != nullptr; ++i){
				if(p == kPermMap[i].zh){
					info.RequestPermissionByName(kPermMap[i].api, p.c_str());
					found = true;
					break;
				}
			}
			if(!found){
				// 未知权限名直接用原字符串申请
				info.RequestPermissionByName(p.c_str(), p.c_str());
			}
		}
	}

	g_appInfoJsonAcp=Utf8ToAcpLossy(info.BuildJsonUtf8());
	// 持久化状态，供事件回调实例恢复
	{
		const std::string stateFile = g_dllDirUtf8 + "\\.xlz_pystate_" + BuildSafePluginDirKey(g_dllPathUtf8.c_str()) + ".txt";
		std::ofstream sf(U8W(stateFile).c_str());
		if(sf){
			sf << g_pyPayloadDirUtf8 << "\n";
			sf << g_pyModuleName << "\n";
			sf << (apiDataJson?apiDataJson:"") << "\n";
			sf << (pluginKeyRaw?pluginKeyRaw:"") << "\n";
		}
	}
	DebugLog(std::string("apprun success, appinfo bytes=")+std::to_string(g_appInfoJsonAcp.size()));
	return g_appInfoJsonAcp.c_str();
}
