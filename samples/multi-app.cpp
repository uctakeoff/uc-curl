/**
uc::curl 

Copyright (c) 2017, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.haxx.se/libcurl/c/multi-app.html
#include <iostream>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <thread>
#include "../uccurl.h"

int main()
{
    try {
        uc::curl::global libcurlInit;

        std::vector<uc::curl::easy> handles;
        handles.push_back(uc::curl::easy("http://example.com"));
        handles.push_back(uc::curl::easy("ftp://example.com"));
        handles.back().setopt<CURLOPT_UPLOAD>();

        uc::curl::multi multi_handle;
        for (auto&& e : handles) {
            multi_handle.add(e);
        }

        uc::curl::fdsets sets;
        while (multi_handle.perform() > 0) {
            multi_handle.fdset(sets);
            auto timeout = std::min(multi_handle.timeout(), std::chrono::milliseconds(1000));
            if (!sets) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else if (sets.select(timeout) == -1) {
                break;
            }
            sets.zero();
        }
            
        multi_handle.for_each_done_info([&](uc::curl::easy_ref&& h, CURLcode result) {
            auto i = std::find(handles.begin(), handles.end(), h);
            std::cout << std::distance(handles.begin(), i) << "th transfer completed with status " << result <<  ": " << h.uri() << "\n";
        });

    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
void multi_app()
{

}

