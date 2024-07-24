#include "Common.h"
#include "Util.hpp"

//状态码
enum StatusCode
{
    OK = 200,
    BAD_REQUEST = 400,  //请求method错误
    NOT_FOUND = 404,    //找不到页面
    SERVER_ERROR = 500  //服务器内部错误
};

const static char* WEB_ROOT = "wwwroot"; // web根目录
const static char* HOME_PAGE = "index.html";    //主页文件名
const static char* END_OF_LINE = "\r\n";   
const static char* BAD_REQUEST_PAGE = "400.html";
const static char* NOT_FOUND_PAGE = "400.html";
const static char* SERVER_ERROR_PAGE = "400.html";

//HttpResponse状态码 转 状态码描述
static std::string StatusCodeTODesc(size_t statusCode)
{
    static std::unordered_map<std::size_t, std::string> statusDescMap = 
    {
        {200, "OK"},
        {400, "BAD_REQUEST"},
        {404, "NOT_FOUND"},
        {500, "SERVER_ERROR"},
    };

    if(statusDescMap.count(statusCode)) //有上述定义的状态码才会返回状态码描述
        return statusDescMap[statusCode];
}

//文件后缀转httpResponse中的Content-Type
static std::string SuffixToDesc(const std::string &suffix)
{
    static std::unordered_map<std::string, std::string> suffixMap = 
    {
        {"html", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"jpg", "application/x-jpg"},
         {"xml", "application/xml"},
    };
    return suffixMap[suffix];
}

//请求报文的相关字段
class HttpRequest
{
public:
    std::string _requestLine;   //请求行
    bool _cgi;  // POST,带参GET，请求资源为可执行程序，用cgi解决
    std::string _method;    //请求方法POST，GET
    std::string _uri;       //uri，/a/b/c/tmp.html
    std::string _path;      //uri路径
    size_t _size;   //非cgi模式响应正文大小，也就是_path对应文件大小
    std::string _suffix;    //_path文件对应后缀
    std::string _queryString;   //GET方法中如果uri带参会用到
    std::string _version;   //版本，如http/1.1
public:
    std::vector<std::string> _requestHeader;    //请求报头，多行，全放vector里面
    std::unordered_map<std::string, std::string> _headkv;   //以kv形式存放所有请求报头
    size_t _contentLenth;  //请求中的正文长度
public:
    std::string _blank;     //空行
    std::string _requestBody;   //请求正文

    HttpRequest(): _cgi(false),_size(0),_contentLenth(0){}
};

//响应报文的相关字段
class HttpResponse
{
public:
    std::string _statusLine;    //状态行
    std::vector<std::string> _responseHeader;    //响应报头，多行，全放vector里面
    std::string _blank;     //空行
    std::string _responseBody;   //响应正文
    size_t _statusCode;
    int _fd;    // 处理非CGI的时候要打开的文件，即调用sendfile要打开的文件
public:
    HttpResponse() : _blank(END_OF_LINE),_statusCode(OK),_fd(-1){}
    ~HttpResponse()
    {
        if(_fd >= 0) close(_fd);    //// 必须关，不然文件描述符泄漏（普通文件）
    }
};

//端点，就是服务器，该类中包含服务端要做的事
class EndPoint
{
    //服务端就做三件事：
    //1. 接受请求并分析请求
    //2. 构建响应
    //3. 返回响应
private:
    int _sock;  //用来通信的sock
    HttpRequest _httprequest;   //对端发来的请求
    HttpResponse _httpResponse;     //当前端的响应
    bool stop;
public:
    EndPoint(int sock)
        :_sock(sock)
        ,stop(false)
        {}
    ~EndPoint(){}
private:// 接收请求内部使用的接口
    //接受请求行
    void RecvRequestLine()
    {
        assert(_sock >= 0);
        if(Util::ReadLine(_sock, _httprequest._requestLine) > 0)
        {
            _httprequest._requestLine.pop_back();
            LOG(INFO, "recv request line ::" + _httprequest._requestLine);
        }
        else{// 读取失败，直接让本次通信停止
            stop = true;
            LOG(ERROR,"RecvRequestLine failed");
            return ;
        }
    }
    // 解析请求行，把 请求方法 uri 协议版本 搞出来
    void parseRequestLine()
    {
        std::stringstream s(_httprequest._requestLine);
        s >> _httprequest._method >>_httprequest._uri >>_httprequest._version;
        // 可能读取到的_method不是大写的，需要手动转为大写
        std::transform(_httprequest._method.begin(), _httprequest._method.end(), _httprequest._method.begin(), ::toupper);
    }

    //接受请求报头
    void RecvRequestHeader()
    {
        // 请求报头中有很多行，都按行读取到vector中
        std::vector<std::string> &header = _httprequest._requestHeader;
        std::string tmp;
        while(true)
        {
            int length = Util::ReadLine(_sock, tmp);
            if(length <= 0)
            {
                stop = true;
                LOG(ERROR,"RecvRequsetHeader faild");
                return ;
            }
            if(length == 1)
            {
                // 只有一个\n，就是空行，直接放到_blank中
                _httprequest._blank = tmp;
                break;
            }
            //不是空行，每个都放进vector
            tmp.pop_back();
            header.push_back(tmp);
            tmp.clear();
        }

    }
    // 请求报头中每一行都是K-V的，所以这里也直接保存成KV的，保存到unoreded_map中
    void PraseRequsetHeader()
    {
        std::string key;
        std::string value;
        for(auto &str : _httprequest._requestHeader)    
        {
            // 请求报头中的KV是以 冒号+空格 分隔的
            if(Util::CutString(str, key, value, ": "))
            {
                _httprequest._headkv.insert({key,value});
            }
            else
            {
                LOG(ERROR, "_httpRequest._requestHeader CutString failed");
            }
        }
    }
    bool IsNeedToRecvRequestBody()
    {
        //只考虑请求方法是POST时才读取正文
        if(_httprequest._method == "POST")
        {
            //请求方法若是POST，那报文中一定会有一个Content-Length字段，根据这个K找到报文长度即可
            _httprequest._contentLenth = std::stoi(_httprequest._headkv["Content-Length"]);
            if(_httprequest._contentLenth != 0)
            {
                LOG(INFO, "POST method, recv Content-Length: " + _httpRequest._headKV["Content-Length"]);
                return true;
            }
            LOG(WARNING, "POST method but recv Content-Lenth is 0");
        }
        return false;
    }

    //接受请求正文
    void RecvRequestBody()
    {
        // 如果请求方法_method是GET，那就一定没有请求正文，如果是POST那就会有请求正文
        if(IsNeedToRecvRequestBody())
        {
            
        }
    }
};
