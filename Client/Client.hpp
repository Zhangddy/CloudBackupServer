#pragma once
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include "httplib.h"
#include <Windows.h>

const DWORD TIME_LAG = 3000;
using namespace std;

class FileUtil
{
public:
	static bool Read(const string& fileName, string& body)
	{
		std::ifstream fs(fileName, std::ios::binary);
		if (fs.is_open() == false)
		{
			cout << "云备份OS: open file error" << endl;
			cout << "云备份OS: Read" << fileName << "error" << endl;
			return false;
		}
		int64_t fsize = boost::filesystem::file_size(fileName);
		body.resize(fsize);
		fs.read(&body[0], fsize);

		if (fs.good() == false)
		{
			cout << "云备份OS: " << fileName << " read error" << endl;
			return false;
		}
		fs.close();
		return true;
	}
	static bool Write(const string& fileName, const string& body)
	{
		// 覆盖写入
		std::ofstream fs(fileName, std::ios::binary);
		if (fs.is_open() == false)
		{
			cout << "云备份OS: open file error" << endl;
			cout << "云备份OS: Write" << fileName << "error" << endl;
			return false;
		}
		fs.write(&body[0], body.size());
		if (fs.good() == false)
		{
			cout << "云备份OS: " << fileName << " write error" << endl;
			return false;
		}
		fs.close();
		return true;
	}
};

class DataManager
{
public:
	DataManager(const string& filepath)
		: _filePath(filepath)
	{

	}

	bool Insert(const string& key, const string& val)
	{
		_backupList[key] = val;
		Save();
		return true;
	}

	bool GetEtag(const string& key, string* val)
	{
		auto it = _backupList.find(key);
		if (it == _backupList.end())
		{
			*val = "未备份文件";
			return false;
		}
		*val = it->second;
		return true;
	}

	bool Save()
	{
		std::stringstream tmp;
		auto it = _backupList.begin();
		for (; it != _backupList.end(); it++)
		{
			tmp << it->first << " " << it->second << "\r\n";
		}
		string str = tmp.str();
		FileUtil::Write(_filePath, str);
		return true;
	}

	bool InitLoad()
	{
		string body;
		if (FileUtil::Read(_filePath, body) == false)
		{
			return false;
		}
		vector<string> list;
		boost::split(list, body, boost::is_any_of("\r\n"), boost::token_compress_off);
		for (auto& e : list)
		{
			size_t pos = e.find(" ");
			if (pos == string::npos)
			{
				continue;
			}
			string key = e.substr(0, pos);
			string val = e.substr(pos + 1);
			Insert(key, val);
		}
		if (!_backupList.empty())
		{
			cout << "云备份OS: 发现本地文件 " << endl;
			cout << "----------------------------" << endl;
		}
		for (auto e : _backupList)
		{
			cout << e.first << "-" << e.second << endl;
		}
		if (!_backupList.empty())
		{
			cout << "----------------------------" << endl;
		}
		cout << endl;
		return true;
	}

private:
	string _filePath;
	unordered_map<string, string> _backupList;
};

class CloudClient
{
public:
	CloudClient(const string& filename, const string& store_file, const string& srv_ip, uint16_t srv_port)
		: _listenDir(filename)
		, _dataManage(store_file)
		, _srvIP(srv_ip)
		, _srvPort(srv_port)
	{ }

	bool Start()
	{
		cout << "云备份OS: 正在进行数据初始化..." << endl;
		_dataManage.InitLoad();
		cout << "云备份OS: 正在监视备份目录..." << endl;
		while (true)
		{
			vector<string> list;
			GetBackupFileList(&list);
			if (!list.empty())
			{
				cout << "云备份OS: 检测到要备份的文件" << endl;
				cout << "----------------------------" << endl;
			}
			for (auto e : list)
			{
				cout << e << endl;
			}
			if (!list.empty())
			{
				cout << "----------------------------" << endl << endl;
			}
			for (size_t i = 0; i < list.size(); i++)
			{
				string name = list[i];
				string pathname = _listenDir + "/" + list[i];
				cout << "云备份OS: 发现文件\"" << pathname << "\"需要备份" << endl;
				string body;
				FileUtil::Read(pathname, body);
				string reqPath = "/" + name;
				httplib::Client client(_srvIP, _srvPort);   
				auto rsp = client.Put(reqPath.c_str(), body, "application/octet-stream");// 正文类型
				if (rsp == NULL || rsp->status != 200)
				{
					cout << "云备份OS: 文件上传失败: " << name << endl;
					if (rsp != NULL)
					{
						cout << "云备份OS: 返回状态码: " << rsp->status << endl;
					}
					continue;
				}
				string etag;
				GetEtag(pathname, &etag);
				// 备份成功更新信息
				_dataManage.Insert(name, etag);
				cout << "云备份OS: " << pathname << " 备份成功!" << endl;
			}
			Sleep(3000);
		}
		return true;
	}

	bool GetBackupFileList(vector<string>* list)
	{
		if (boost::filesystem::exists(_listenDir) == false)
		{
			boost::filesystem::create_directories(_listenDir);
			return true;
		}

		boost::filesystem::directory_iterator begin(_listenDir);
		boost::filesystem::directory_iterator end;
		for (; begin != end; ++begin)
		{
			// 目录文件不进行读取
			if (boost::filesystem::is_directory(begin->status()))
			{
				continue;
			}
			string pathname = begin->path().string();
			string name = begin->path().filename().string();
			string etag;
			GetEtag(pathname, &etag);
			string oldEtag;
			bool ret = _dataManage.GetEtag(name, &oldEtag);
			if (ret == false || etag != oldEtag)
			{
				list->push_back(name);
			}
		}
		return true;
	}

	static bool GetEtag(const string& pathName, string* etag)
	{
		int64_t fileSize = boost::filesystem::file_size(pathName);
		time_t lastTime = boost::filesystem::last_write_time(pathName);
		string tmp = "-";
		*etag = to_string(fileSize) + tmp + to_string(lastTime);
		return true;
	}
private:
	string _listenDir;
	DataManager _dataManage;
	string _srvIP;
	uint16_t _srvPort;
};
