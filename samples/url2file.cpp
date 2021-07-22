/**
uc::curl 

Copyright (c) 2021, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/

// See https://curl.se/libcurl/c/url2file.html
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "../uccurl.h"

/* <DESC>
 * Download a given URL into a local file named page.out.
 * </DESC>
 */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << "<URL>\n";
        return 1;
    }
    try {
        uc::curl::global libcurlInit;

        uc::curl::easy(argv[1])            // set URL to get here
            .setopt<CURLOPT_VERBOSE>()     // Switch on full protocol/debug output while testing
            .setopt<CURLOPT_NOPROGRESS>()  // disable progress meter, set to 0L to enable it
            >> std::ofstream("page.out", std::ios::out | std::ios::binary);

    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}