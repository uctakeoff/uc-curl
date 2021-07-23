/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.haxx.se/libcurl/c/multi-single.html
#include <iostream>
#include <stdexcept>
#include <chrono>
#include "../uccurl.h"

int main()
{
    try {
        uc::curl::global libcurlInit;

        uc::curl::easy http_handle("http://www.example.com/");

        uc::curl::multi multi_handle;
        multi_handle.add(http_handle);

        while (multi_handle.perform() > 0) {
            multi_handle.poll(std::chrono::seconds{1});
        }

        multi_handle.remove(http_handle);
    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
