#include "profiler.h"
#include <iostream>
MyProfiler::Profiler* MyProfiler::Profiler::ptr = nullptr;

MyProfiler::Block::Block(const std::string& _name, Block* _parent)
	:name(_name),
	Parent(_parent)
{
	start = std::chrono::steady_clock::now();
}

MyProfiler::Block::~Block()
{
	//delete all the children
	for (auto iter : children)
		delete iter;
	children.clear();
}

double MyProfiler::Block::GetSeconds()const
{
	return std::chrono::duration<double>(end - start).count();
}

std::string MyProfiler::Block::GetName()
{
	return name;
}

void MyProfiler::Block::End()
{
	end = std::chrono::steady_clock::now();
}

MyProfiler::Block* MyProfiler::Block::AddChild(const std::string& _name)
{
	Block* p = new Block(_name, this);
	children.push_back(p);
	return p;
}

void MyProfiler::Block::Dump(int n)const
{
	//print correct ammount of tabs
	for (int i = 0; i < n; i++)
	{
		std::cout << "\t";
	}
	//print name and seconds
	std::cout << name << " in" << GetSeconds() << " seconds" << std::endl;
	//print children
	for (const auto* c : children)
		c->Dump(n + 1);
}

MyProfiler::Profiler::~Profiler()
{
	Clear();
}

void MyProfiler::Profiler::StartBlock(std::string _name)
{
	if (!current)
		current = new Block(_name); //I am root	
	else
		current = current->AddChild(_name);
}

void MyProfiler::Profiler::End()
{
	//current가 nullptr이면 크래쉬임
	//stop counting time on current block
	current->End();
	//go 1 step back	
	Block* parent = current->GetParent();

	//if no parent == root node. Push current to Fully Finished
	if (!parent)
		FullyFinishedBlocks.push_back(current);
	current = parent;
}

void MyProfiler::Profiler::Dump()
{
	for (const auto* b : FullyFinishedBlocks)
	{
		b->Dump();
	}
}

void MyProfiler::Profiler::Clear()
{
	//iterate end UNTIL current in nullptr
	while (current)
		End();
	//delete all the finished nodes

	for (auto iter : FullyFinishedBlocks)
		delete iter;

	FullyFinishedBlocks.clear();
}

MyProfiler::Profiler* MyProfiler::Profiler::GetPtr()
{
	if (!ptr)
		ptr = new Profiler;
	return ptr;
}

void MyProfiler::Profiler::DeletePtr()
{
	if (ptr)
		delete ptr;
	ptr = nullptr;
}

