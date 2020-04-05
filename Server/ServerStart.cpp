#include "Server.hpp"
#include <thread>


using namespace std;

void compressFunc()
{
    NonHotCompress ncom(GZFILE_DIR, BACKUP_DIR);
    ncom.Start();
}

void httpFunc()
{
    Server srv;
    srv.Start();
}

int main()
{
    if (boost::filesystem::exists(GZFILE_DIR) == false)
    {
        boost::filesystem::create_directory(GZFILE_DIR);
    }
    if (boost::filesystem::exists(BACKUP_DIR) == false)
    {
        boost::filesystem::create_directory(BACKUP_DIR);
    }
    pthread_mutex_init(&mtx, NULL);

    thread compress(compressFunc);
    thread server(httpFunc);
    compress.join();
    server.join();
    pthread_mutex_destroy(&mtx);

    return 0;
}
