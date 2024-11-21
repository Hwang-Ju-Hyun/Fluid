#pragma once
#include <string>
#include <chrono>
#include <list>

namespace MyProfiler
{
	class Block
	{
		std::string name;
		std::chrono::steady_clock::time_point start;
		std::chrono::steady_clock::time_point end;


		std::list<Block*> children;
		Block* Parent;
	public:
		Block(const std::string& _name, Block* _parent = nullptr);
		~Block();
	public:
		double GetSeconds()const;
		std::string GetName();
		void End();
		Block* AddChild(const std::string& _name);
		Block* GetParent() { return Parent; }

		void Dump(int n = 0)const;
	};

	class Profiler
	{
	private:
		static Profiler* ptr;
		Profiler() = default;
		Profiler(const Profiler&) = default;
		~Profiler();
		Profiler& operator=(const Profiler&) = default;
		Block* current = nullptr;

		std::list<Block*> FullyFinishedBlocks;
	public:
		void StartBlock(std::string _name);
		void End();
		void Dump();

		void Clear();

		static Profiler* GetPtr();
		static void DeletePtr();
	};
}

#ifdef _DEBUG
#define DEBUG_PROFILER_START(x) MyProfiler::Profiler::GetPtr()->StartBlock(x);
#define DEBUG_PROFILER_END MyProfiler::Profiler::GetPtr()->End();
#define DEBUG_PROFILER_DUMP MyProfiler::Profiler::GetPtr()->Dump();
#define DEBUG_PROFILER_DELETE MyProfiler::Profiler::GetPtr()->DeletePtr();
#endif 


//#ifndef __FUNTION_NAME__
//#ifdef WIN32
//#define __FUNTION_NAME__ __FUNCTION__
//#else
//#define _FUNCTION_NAME __func__
//#endif
//#endif
