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
			cout << "�Ʊ���OS: open file error" << endl;
			cout << "�Ʊ���OS: Read" << fileName << "error" << endl;
			return false;
		}
		int64_t fsize = boost::filesystem::file_size(fileName);
		body.resize(fsize);
		fs.read(&body[0], fsize);

		if (fs.good() == false)
		{
			cout << "�Ʊ���OS: " << fileName << " read error" << endl;
			return false;
		}
		fs.close();
		return true;
	}
	static bool Write(const string& fileName, const string& body)
	{
		// ����д��
		std::ofstream fs(fileName, std::ios::binary);
		if (fs.is_open() == false)
		{
			cout << "�Ʊ���OS: open file error" << endl;
			cout << "�Ʊ���OS: Write" << fileName << "error" << endl;
			return false;
		}
		fs.write(&body[0], body.size());
		if (fs.good() == false)
		{
			cout << "�Ʊ���OS: " << fileName << " write error" << endl;
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
			*val = "δ�����ļ�";
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
			cout << "�Ʊ���OS: ���ֱ����ļ� " << endl;
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
		cout << "�Ʊ���OS: ���ڽ������ݳ�ʼ��..." << endl;
		_dataManage.InitLoad();
		cout << "�Ʊ���OS: ���ڼ��ӱ���Ŀ¼..." << endl;
		while (true)
		{
			vector<string> list;
			GetBackupFileList(&list);
			if (!list.empty())
			{
				cout << "�Ʊ���OS: ��⵽Ҫ���ݵ��ļ�" << endl;
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
				cout << "�Ʊ���OS: �����ļ�\"" << pathname << "\"��Ҫ����" << endl;
				string body;
				FileUtil::Read(pathname, body);
				string reqPath = "/" + name;
				httplib::Client client(_srvIP, _srvPort);   
				auto rsp = client.Put(reqPath.c_str(), body, "application/octet-stream");// ��������
				if (rsp == NULL || rsp->status != 200)
				{
					cout << "�Ʊ���OS: �ļ��ϴ�ʧ��: " << name << endl;
					if (rsp != NULL)
					{
						cout << "�Ʊ���OS: ����״̬��: " << rsp->status << endl;
					}
					continue;
				}
				string etag;
				GetEtag(pathname, &etag);
				// ���ݳɹ�������Ϣ
				_dataManage.Insert(name, etag);
				cout << "�Ʊ���OS: " << pathname << " ���ݳɹ�!" << endl;
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
			// Ŀ¼�ļ������ж�ȡ
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
