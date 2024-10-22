#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>       // open
#include <unistd.h>      // close
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "buffer/buffer.h"
#include "log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(CircleBuffer& buff);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(CircleBuffer& buff, std::string message);
    int Code() const { return code_; }

private:
    void AddStateLine_(CircleBuffer &buff);
    void AddHeader_(CircleBuffer &buff);
    void AddContent_(CircleBuffer &buff);

    void ErrorHtml_();
    std::string GetFileType_();

    int code_;
    bool isKeepAlive_;
    int Errno;

    // 请求的资源路径
    std::string path_;
    // 资源根目录
    std::string srcDir_;
    
    // mmap映射的文件指针，响应静态资源
    char* mmFile_; 
    // 文件状态信息
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 后缀类型集
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 编码状态集
    static const std::unordered_map<int, std::string> CODE_PATH;            // 编码路径集
};


#endif //HTTP_RESPONSE_H
