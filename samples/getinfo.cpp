/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

#include <iostream>
#include <stdexcept>
#include "../uccurl.h"

std::ostream& operator<<(std::ostream& os, const uc::curl::slist& obj)
{
    for (const char* e : obj) {
        os << e << ", ";
    }
    return os;
}
std::ostream& operator<<(std::ostream& os, const curl_certinfo* obj)
{
    os << "num=" << obj->num_of_certs << ", ";
    for (auto n = 0; n < obj->num_of_certs; ++n) {
        for (uc::curl::slist_iterator i{obj->certinfo[n]}, e{}; i != e; ++i) {
            os << *i << ", ";
        }
    }
    return os;
}
std::ostream& operator<<(std::ostream& os, const curl_tlssessioninfo* obj)
{
    return os << "backend=" << obj->backend << ",internals=" << obj->internals;
}

template<typename T, typename std::enable_if<std::is_pointer<T>::value, std::nullptr_t>::type = nullptr> void print(T&& v)
{
    if (v) std::cout << v << std::endl;
    else std::cout << "(null)" << std::endl;
}
template<typename T, typename std::enable_if<!std::is_pointer<T>::value, std::nullptr_t>::type = nullptr> void print(T&& v)
{
    std::cout << v << std::endl;
}

int main()
{
    try {
        uc::curl::easy curl("https://example.com");
        curl >> [](const char* ptr, size_t nbytes) { return nbytes; };

        std::cout << curl.uri() << std::endl;
        std::cout << curl.get_socket() << std::endl;

#define PRINT_CURL_INFO(i) std::cout << #i << "\t\t: "; print(curl.getinfo<i>())
        PRINT_CURL_INFO(CURLINFO_EFFECTIVE_URL);
        PRINT_CURL_INFO(CURLINFO_RESPONSE_CODE);
        PRINT_CURL_INFO(CURLINFO_TOTAL_TIME);
        PRINT_CURL_INFO(CURLINFO_NAMELOOKUP_TIME);
        PRINT_CURL_INFO(CURLINFO_CONNECT_TIME);
        PRINT_CURL_INFO(CURLINFO_PRETRANSFER_TIME);
        PRINT_CURL_INFO(CURLINFO_SIZE_UPLOAD);
        PRINT_CURL_INFO(CURLINFO_SIZE_UPLOAD_T);
        PRINT_CURL_INFO(CURLINFO_SIZE_DOWNLOAD);
        PRINT_CURL_INFO(CURLINFO_SIZE_DOWNLOAD_T);
        PRINT_CURL_INFO(CURLINFO_SPEED_DOWNLOAD);
        PRINT_CURL_INFO(CURLINFO_SPEED_DOWNLOAD_T);
        PRINT_CURL_INFO(CURLINFO_SPEED_UPLOAD);
        PRINT_CURL_INFO(CURLINFO_SPEED_UPLOAD_T);
        PRINT_CURL_INFO(CURLINFO_HEADER_SIZE);
        PRINT_CURL_INFO(CURLINFO_REQUEST_SIZE);
        PRINT_CURL_INFO(CURLINFO_SSL_VERIFYRESULT);
        PRINT_CURL_INFO(CURLINFO_FILETIME);
        PRINT_CURL_INFO(CURLINFO_FILETIME_T);
        PRINT_CURL_INFO(CURLINFO_CONTENT_LENGTH_DOWNLOAD);
        PRINT_CURL_INFO(CURLINFO_CONTENT_LENGTH_DOWNLOAD_T);
        PRINT_CURL_INFO(CURLINFO_CONTENT_LENGTH_UPLOAD);
        PRINT_CURL_INFO(CURLINFO_CONTENT_LENGTH_UPLOAD_T);
        PRINT_CURL_INFO(CURLINFO_STARTTRANSFER_TIME);
        PRINT_CURL_INFO(CURLINFO_CONTENT_TYPE);
        PRINT_CURL_INFO(CURLINFO_REDIRECT_TIME);
        PRINT_CURL_INFO(CURLINFO_REDIRECT_COUNT);
        PRINT_CURL_INFO(CURLINFO_PRIVATE);
        PRINT_CURL_INFO(CURLINFO_HTTP_CONNECTCODE);
        PRINT_CURL_INFO(CURLINFO_HTTPAUTH_AVAIL);
        PRINT_CURL_INFO(CURLINFO_PROXYAUTH_AVAIL);
        PRINT_CURL_INFO(CURLINFO_OS_ERRNO);
        PRINT_CURL_INFO(CURLINFO_NUM_CONNECTS);
        PRINT_CURL_INFO(CURLINFO_SSL_ENGINES);
        PRINT_CURL_INFO(CURLINFO_COOKIELIST);
        PRINT_CURL_INFO(CURLINFO_LASTSOCKET);
        PRINT_CURL_INFO(CURLINFO_FTP_ENTRY_PATH);
        PRINT_CURL_INFO(CURLINFO_REDIRECT_URL);
        PRINT_CURL_INFO(CURLINFO_PRIMARY_IP);
        PRINT_CURL_INFO(CURLINFO_APPCONNECT_TIME);
        PRINT_CURL_INFO(CURLINFO_CERTINFO);
        PRINT_CURL_INFO(CURLINFO_CONDITION_UNMET);
        PRINT_CURL_INFO(CURLINFO_RTSP_SESSION_ID);
        PRINT_CURL_INFO(CURLINFO_RTSP_CLIENT_CSEQ);
        PRINT_CURL_INFO(CURLINFO_RTSP_SERVER_CSEQ);
        PRINT_CURL_INFO(CURLINFO_RTSP_CSEQ_RECV);
        PRINT_CURL_INFO(CURLINFO_PRIMARY_PORT);
        PRINT_CURL_INFO(CURLINFO_LOCAL_IP);
        PRINT_CURL_INFO(CURLINFO_LOCAL_PORT);
        PRINT_CURL_INFO(CURLINFO_TLS_SESSION);
        PRINT_CURL_INFO(CURLINFO_ACTIVESOCKET);
        PRINT_CURL_INFO(CURLINFO_TLS_SSL_PTR);
        PRINT_CURL_INFO(CURLINFO_HTTP_VERSION);
        PRINT_CURL_INFO(CURLINFO_PROXY_SSL_VERIFYRESULT);
        PRINT_CURL_INFO(CURLINFO_PROTOCOL);
        PRINT_CURL_INFO(CURLINFO_SCHEME);
        PRINT_CURL_INFO(CURLINFO_TOTAL_TIME_T);
        PRINT_CURL_INFO(CURLINFO_NAMELOOKUP_TIME_T);
        PRINT_CURL_INFO(CURLINFO_CONNECT_TIME_T);
        PRINT_CURL_INFO(CURLINFO_PRETRANSFER_TIME_T);
        PRINT_CURL_INFO(CURLINFO_STARTTRANSFER_TIME_T);
        PRINT_CURL_INFO(CURLINFO_REDIRECT_TIME_T);
        PRINT_CURL_INFO(CURLINFO_APPCONNECT_TIME_T);
        PRINT_CURL_INFO(CURLINFO_RETRY_AFTER);

        // PRINT_CURL_INFO(CURLINFO_EFFECTIVE_METHOD);
        // PRINT_CURL_INFO(CURLINFO_PROXY_ERROR);
        // PRINT_CURL_INFO(CURLINFO_REFERER);

    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
