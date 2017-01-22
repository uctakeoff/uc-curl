/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.haxx.se/libcurl/c/getinmemory.html
#include <iostream>
#include <stdexcept>
#include <string>
#include "../uccurl.h"

int main()
{
    try {
        uc::curl::global libcurlInit;

        std::string chunk;
        uc::curl::easy("http://www.example.com/").setopt<CURLOPT_USERAGENT>("libcurl-agent/1.0") >> chunk;
        std::cout << chunk.size() << " bytes retrieved\n";

    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
