#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <zlib.h>
#include <pthread.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "httplib.h"

using std::string;
using std::unordered_map;
using std::vector;
using std::cout;
using std::endl;

constexpr auto NONHOT_TIME = 10;
constexpr auto INTERVAL_TIME = 20 ;
constexpr auto BACKUP_DIR = "./backup/";
constexpr auto GZFILE_DIR = "./gzfile/" ;
constexpr auto DATA_FILE = "./list.backup";

class FileUtil
{
    public:
        static bool Read(const string& fileName, string& body)
        {
            std::ifstream fs(fileName, std::ios::binary); 
            if (fs.is_open() == false)
            {
              cout << "open file error" << endl; 
              return false;
           }
           int64_t fsize = boost::filesystem::file_size(fileName);
           body.resize(fsize);
           fs.read(&body[0], fsize);
           if (fs.good() == false)
           {
               cout << fileName << " read error" << endl;
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
              cout << "open file error" << endl; 
              cout << "Write error" << endl;
              return false;
           }
           fs.write(&body[0], body.size());
           if (fs.good() == false)
           {
               cout << fileName << " write error" << endl;
               return false;
           }
           fs.close();
           return true;
        }
};

class CompressUtil
{
    public:
        static bool Compress(const string& srcFile, const string& destFile)
        {
            string body;
            FileUtil::Read(srcFile, body);

            gzFile gf = gzopen(destFile.c_str(), "wb");
            if (gf == NULL)
            {
                cout << "open file " << destFile << " error" << endl;
                
                cout << 3 << endl;
                return false;
            }

            size_t wlen = 0;
            while (wlen < body.size())
            {
                int ret = gzwrite(gf, &body[wlen], body.size() - wlen);
                if (ret == 0)
                {
                    cout << "file " << destFile << " gzwrite error" << endl;
                    return false;
                }
                wlen += ret;
            }
            gzclose(gf);
            return true;
        }

        static bool UnCompress(const string& srcFile, const string& destFile)
        {
            std::ofstream ofs(destFile, std::ios::binary);
            if (ofs.is_open() == false)
            {
                cout << "open file " << destFile << " error" << endl;
                cout << 1 << endl;
                return false;
            }
            gzFile gf = gzopen(srcFile.c_str(), "rb");
            if (gf == NULL)
            {
                cout << "open file " << srcFile << "error" << endl;
                cout << 2 << endl;
                ofs.close();
                return false;
            }

            char tmp[4096] = { 0 };
            int ret = 0;
            while ((ret = gzread(gf, tmp, 4096)) > 0)
            {
                ofs.write(tmp, ret);
            }
            ofs.close();
            gzclose(gf);
            return true;
        }
};


class DataManger
{
    public:
        DataManger(const string& path)
            : _filePath(path)
        {
            pthread_rwlock_init(&_rwlock, NULL);
        }

        ~DataManger()
        {
            pthread_rwlock_destroy(&_rwlock);
        }

        bool IsExists(const string& fileName)
        {
            // 判断是否能在_fileList中找到
            pthread_rwlock_rdlock(&_rwlock);
            auto it = _fileList.find(fileName);
            if (it == _fileList.end())
            {
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }

        bool IsCompress(const string& fileName)
        {
            pthread_rwlock_rdlock(&_rwlock);
            auto it = _fileList.find(fileName);
            if (it == _fileList.end())
            {
                return false;
            }
            if (it->first == it->second)
            {
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }

        bool GetNonCompressList(vector<string>& List)
        {
            pthread_rwlock_rdlock(&_rwlock);
            auto it = _fileList.begin();
            while (it != _fileList.end())
            {
                if (it->first == it->second)
                {
                    List.push_back(it->first);
                }
                it++;
            }
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }

        bool Insert(const string& first, const string& second)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _fileList[first] = second;
            pthread_rwlock_unlock(&_rwlock);
            Upload();
            return true;
        }

        bool GetList(vector<string>& list)
        {
            pthread_rwlock_rdlock(&_rwlock);
            auto it = _fileList.begin();
            for(; it != _fileList.end(); it++)
            {
                list.push_back(it->first);
            }
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }

        bool GetGzName(const string& src, string& dest)
        {
            auto it = _fileList.find(src);
            if (it == _fileList.end())
            {
                return false;
            }
            dest = it->second;
            return true;
        }

        // 将文件信息保存
        bool Upload()
        {
            std::stringstream tmp;
            pthread_rwlock_rdlock(&_rwlock);
            auto it = _fileList.begin();
            for(; it != _fileList.end(); it++)
            {
                tmp << it->first << " " << it->second << "\r\n";
            }
            pthread_rwlock_unlock(&_rwlock);
            string str = tmp.str();
            FileUtil::Write(_filePath, str);
            return true;
        }

        // 初始化列表
        bool Download()
        {
            string body;
            if (FileUtil::Read(_filePath, body) == false)
            {
                cout << "无初始数据!" << endl;
                return false;
            }
            vector<string> list;
            boost::split(list, body, boost::is_any_of("\r\n"), boost::token_compress_off);
            cout << "初始化数据成功!" << endl;

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
            cout << "初始化列表如下: " << endl;
            for (auto&e : _fileList)
            {
                cout << e.first << " " << e.second << endl;
            }
            cout << "-----------" << endl;
            return true;
        }

    private:
        string _filePath;
        unordered_map<string, string> _fileList;
        pthread_rwlock_t _rwlock;
};

DataManger data_manger(DATA_FILE);
pthread_mutex_t mtx;
class Server
{
    public:
        // 启动网络通信模块
        bool Start()
        {
            // 上传文件
            // 得到文件列表
            // 下载文件
            cout << "云备份OS: 云备份服务器已启动!" << endl;
            cout << "云备份OS: 正在监听..." << endl;
            _server.Put("/(.*)", FileUpload);
            _server.Get("/list", FileList);
            _server.Get("/download/(.*)", FileDownload);

            _server.listen("0.0.0.0", 9000);
            return true;
        }

    private:
        // 文件上传处理回调函数
        static void FileUpload(const httplib::Request& req, httplib::Response& rsp)
        {
            cout << "云备份OS: 接受到文件上传请求..." << endl;
            pthread_mutex_lock(&mtx);
            string filename = req.matches[1];
            string pathname = BACKUP_DIR + filename;
            FileUtil::Write(pathname, req.body);
            data_manger.Insert(filename, filename);
            rsp.status = 200;
            pthread_mutex_unlock(&mtx);
            return;
        }
        // 获得文件列表回调函数
        static void FileList(const httplib::Request& req, httplib::Response& rsp)
        {
            cout << "云备份OS: 接受到文件列表请求..." << endl;
            pthread_mutex_lock(&mtx);
            vector<string> list;
            data_manger.GetList(list);
            std::stringstream tmp;
            tmp << "<html><body><hr />";
            for (size_t i = 0; i < list.size(); i++)
            {
                tmp << "<a href='/download/" << list[i] << "'>" << list[i] << "</a>";
                tmp << "<hr />";
            }
            if (list.size() == 0)
            {
                tmp << "The server has no files! "<< "<hr /></body></html>";
            }
            rsp.set_content(tmp.str().c_str(), tmp.str().size(), "text/html");
            rsp.status = 200;
            pthread_mutex_unlock(&mtx);
            return;
        }
        // 文件下载回调函数
        static void FileDownload(const httplib::Request& req, httplib::Response& rsp)
        {
            cout << "云备份OS: 接受到文件文件下载请求..." << endl;
            pthread_mutex_lock(&mtx);
            // 前边路由注册时捕捉的(.*) 即文件名
            string filename = req.matches[1];
            // 不在服务器中
            if (data_manger.IsExists(filename) == false)
            {
                rsp.status = 404;
                pthread_mutex_unlock(&mtx);
                return;
            }
            // 在服务器中并被压缩了, 进行解压
            string pathname = BACKUP_DIR + filename;
            if (data_manger.IsCompress(filename) == true)
            {
                string gzfile;
                data_manger.GetGzName(filename, gzfile);
                string gzpathname = GZFILE_DIR + gzfile;
                CompressUtil::UnCompress(gzpathname, pathname);
                data_manger.Insert(filename, filename);
                unlink(gzpathname.c_str());
            } 

            FileUtil::Read(pathname, rsp.body);    
            rsp.set_header("Content-Type", "application/octet-stream");
            rsp.status = 200;
            pthread_mutex_unlock(&mtx);
            return;
        }
    private:
        string _filePath;
        httplib::Server _server;
};


class NonHotCompress
{
    public:
        NonHotCompress(const string gzPath, const string filePath)
            : _zipFilePath(gzPath)
            , _filePath(filePath)
        {  }

        bool Start()
        {
            while (1)
            {
                pthread_mutex_lock(&mtx);
                // 1. 获取所有未压缩文件文件列表
                vector<string> list;
                data_manger.GetNonCompressList(list); 
                
                // 2. 逐个判断这个文件是否是热点文件
                for (size_t i = 0; i < list.size(); i++)
                {
                    bool ret = IsHotFile(_filePath + list[i]);
                    if (ret == false)
                    {
                        // 对非热点文件进行压缩
                        string srcFileName = list[i];
                        string destFileName = list[i] + ".gz";
                        string srcPath = _filePath + srcFileName;
                        string destPath = _zipFilePath + destFileName;
                        cout << "云备份OS: 正在压缩 " << list[i] << endl;;
                        if (CompressUtil::Compress(srcPath, destPath) == true)
                        {
                            data_manger.Insert(srcFileName, destFileName);
                            unlink(srcPath.c_str());
                            cout << "云备份OS: "<< list[i] << "压缩成功!";
                        }
                        cout << endl;
                    }
                }
                pthread_mutex_unlock(&mtx);
                sleep(INTERVAL_TIME);
            }
            return true;
        }
    private:
        bool IsHotFile(const string& fileName)
        {
            // 非热点文件判断: 当前时间减去最后一次访问时间
            time_t cur_t = time(NULL);
            struct stat st;
            if (stat(fileName.c_str(), &st) < 0)
            {
                cout << "get file " << fileName << " statu failed!\n" << endl;
                return false;
            }
            if ((cur_t - st.st_atime) > NONHOT_TIME)
            {
                return false;
            }
            return true;
        }
    private:
        string _zipFilePath;
        string _filePath; // 压缩前路径
};

