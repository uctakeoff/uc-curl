/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.haxx.se/libcurl/c/multi-single.html
#include <iostream>
#include <stdexcept>
#include "../uccurl.h"

#ifdef _WIN32
#define WAITMS(x) Sleep(x)
#else
/* Portable sleep for platforms other than Windows. */ 
#define WAITMS(x) {                             \
  struct timeval wait = uc::curl::msec_to_timeval(x);      \
  (void)select(0, NULL, NULL, NULL, &wait);     \
}
#endif

int main()
{
    try {
        uc::curl::global libcurlInit;

        uc::curl::easy http_handle("http://www.example.com/");

        uc::curl::multi multi_handle;
        multi_handle.add(http_handle);

        while (multi_handle.perform() > 0) {
            if (multi_handle.wait(1000) == 0) {
                WAITMS(100);
            }
        }
    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
