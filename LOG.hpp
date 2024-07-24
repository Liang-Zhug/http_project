#pragma once

#include<iostream>

using std::cout;
using std::endl;

enum LogLevel
{
	INFO,
	WARNING,
	ERROR,
	FATAL
};

// 宏替换能保证每次调用的地方都是对应的文件和行
#define LOG(level, message) Log(#level, message, __FILE__, __LINE__) // 打印日志

void Log(const std::string& level,const std::string& message,const std::string& fileName, int line)
{
// 日志格式：
// [level]<file:line>{日志内容}==>time
	
	time_t tm = time(nullptr);
	cout << "[" << level << "](" << fileName << ":" << line << ")" << "{" << message << "}==> " << ctime(&tm);
}