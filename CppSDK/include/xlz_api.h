#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace xlz
{
	// API函数地址解析器（从apidata/json中按中文函数名取地址）
	class ApiResolver
	{
	public:
		// 设置API数据（原始：apidata/jsonstr）
		void SetApiData(const char* apiDataJson);

		// 取API函数地址（原始：取API函数地址）
		uintptr_t GetAddress(const char* apiChineseName) const;

	private:
		std::string apiData_;
	};

	// x86 stdcall通用调用器（原始：调用子程序）
	class StdCallInvoker
	{
	public:
		// 调用并返回32位整数（原始：长整数型/整数型返回）
		static uint32_t CallU32(uintptr_t address, const std::vector<uint32_t>& args);

		// 调用并返回64位整数（原始：长整数型返回）
		static uint64_t CallU64(uintptr_t address, const std::vector<uint32_t>& args);

		// 调用并返回指针（原始：到整数/指针返回）
		static void* CallPtr(uintptr_t address, const std::vector<uint32_t>& args);

		// 调用无返回（原始：调用子程序无返回）
		static void CallVoid(uintptr_t address, const std::vector<uint32_t>& args);
	};

	// 参数打包器（把各种类型按x86栈布局拆成DWORD序列）
	class ArgPacker
	{
	public:
		// 压入32位整数（原始：整数型/逻辑型/指针）
		void PushU32(uint32_t v);

		// 压入64位整数（原始：长整数型）
		void PushU64(uint64_t v);

		// 压入double（原始：双精度）
		void PushF64(double v);

		// 压入指针（原始：到整数(&x)）
		void PushPtr(const void* p);

		// 取参数DWORD数组
		const std::vector<uint32_t>& Data() const;

	private:
		std::vector<uint32_t> args_;
	};
}

