# uc::curl

uc::curl is a libcurl wrapper library created C++11 single-header.
It depends only on libcurl and STL.

```cpp:sample.cpp
#include <iostream>
#include <string>
#include "uccurl.h"

int main()
{
    try {
        uc::curl::global libcurlInit;

        std::string data;
        uc::curl::easy("http://www.example.com/") >> data;

    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}

```

build
```bash
$ g++ sample.cpp -std=c++11 -lcurl
```

## License

MIT-Lisence

## EASY interface

### Simple to use.

[sample.c](https://curl.haxx.se/libcurl/c/simple.html) above is roughly rewritten as follows.

```cpp
// See https://curl.haxx.se/libcurl/c/simple.html
#include <iostream>
#include "uccurl.h"

int main()
{
    try {
        uc::curl::easy("http://example.com").perform();
    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
```


`uc::curl::easy("http://example.com")` has the same effect as:

```c
CURL* curl = curl_easy_init();
curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");
curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 20L);
```

The value of [`CURLOPT_MAXREDIRS`](https://curl.haxx.se/libcurl/c/CURLOPT_MAXREDIRS.html) can be specified as the second argument. The default value is 20.

* `uc::curl::easy("http://example.com", 0)` will make libcurl refuse any redirect.
* `uc::curl::easy("http://example.com", -1)`  for an infinite number of redirects.

### setopt

There are several ways.

```cpp
    uc::curl::easy curl;
    curl.setopt(CURLOPT_VERBOSE, 1L);       // OK: Compatible with conventional
    curl.setopt<CURLOPT_VERBOSE>(1L);       // OK: argument type check
    curl.setopt<CURLOPT_VERBOSE>();         // OK: If no argument is specified, 1L is specified.
//  curl.setopt<CURLOPT_VERBOSE>("text");   // Complie error

    std::string url("http://example.com");
    curl.uri(url);                          // Dedicated function
    curl.setopt(CURLOPT_URL, url.c_str());
    curl.setopt<CURLOPT_URL>(url);
```

### getinfo

Automatically resolve type.
```cpp
    const char* url = curl.url();  // getinfo<CURLINFO_EFFECTIVE_URL>();

    const char* type = curl.getinfo<CURLINFO_CONTENT_TYPE>();
    long code = curl.getinfo<CURLINFO_RESPONSE_CODE>();
    double time = curl.getinfo<CURLINFO_TOTAL_TIME>();
    curl_certinfo* info = curl.getinfo<CURLINFO_CERTINFO>();
    uc::curl::slist list = curl.getinfo<CURLINFO_COOKIELIST>();
```

### **GET** function

You can use `std::string`, `std::ostream`, `size_t(const char*, size_t)`  for the `operator>>()`.

```cpp
    uc::curl::easy curl("http://example.com");

    // output to cerr
    curl >> std::cerr;

    // output file
    curl >> std::ofstream("page.out", std::ios::out | std::ios::binary);

    // get in memory
    std::string response;
    curl >> response;

    // callback function
    curl >> [](const char* ptr, size_t size) {
            std::cout << "## receive : " << size << "bytes\n"
                << std::string(ptr, size) << "\n\n";
            return size;
        };
```

`operator>>()` performs the following processing in order.

1. Set [`CURLOPT_WRITEDATA`](https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html) and [`CURLOPT_WRITEFUNCTION`](https://curl.haxx.se/libcurl/c/CURLOPT_WRITEDATA.html).
1. Call `uc::curl::easy::perform()`.
1. Clear  [`CURLOPT_WRITEDATA`](https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html) and [`CURLOPT_WRITEFUNCTION`](https://curl.haxx.se/libcurl/c/CURLOPT_WRITEDATA.html).


### **POST** function

[simplepost.c](https://curl.haxx.se/libcurl/c/simplepost.html) above is roughly rewritten as follows.

```cpp
    // POST string
    uc::curl::easy("http://example.com").postfields("moo mooo moo moo").perform();
```

`postfields()` can take `std::string`, `std::istream`, `uc::curl::form` as arguments.

```cpp
    uc::curl::easy curl("http://example.com/")

    // POST file
    std::ifstream is("formdata.txt", std::ios::in | std::ios::binary);
    curl.postfields(is).perform();

    // POST form data
    uc::curl::form formpost;
    formpost
        .file("sendfile", "postit2.c")
        .contents("filename", "postit2.c")
        .contents("submit", "send");    
    curl.postfields(formpost).perform();
```

`uc::curl::form` is a `struct curl_httppost` wrapper using `std::unique_ptr`.

### **PUT** function

```cpp
    std::ifstream is(filename, std::ios::in | std::ios::binary);
    uc::curl::easy(uri).setopt<CURLOPT_UPLOAD>().body(is).perform();
```

### other function

```cpp
    std::string url;

    // HEAD
    // You can use `std::string`, `std::ostream`, `size_t(const char*, size_t)`  for the `response_header()`.
    auto resheader = [](const char* ptr, size_t size) {
        std::cout << "###" << std::string(ptr, size);
        return size;
    };
    uc::curl::easy(url).setopt<CURLOPT_NOBODY>().response_header(resheader).perform();
        

    // DELETE
    uc::curl::easy(url).setopt<CURLOPT_CUSTOMREQUEST>("DELETE").perform();
```

### uc::curl::slist

`uc::curl::slist` is a `struct curl_slist` wrapper using `std::unique_ptr`. 


set sample
```cpp
    auto chunk = uc::curl::create_slist(
        "Accept:", 
        "Another: yes", 
        "Host: example.com", 
        "X-silly-header;"); 
    uc::curl::easy(url).header(chunk).perform();
```

get sample
```cpp
    uc::curl::slist list = curl.getinfo<CURLINFO_COOKIELIST>();

    // e : const char*
    for (auto&& e : list) {
        std::cout << e << "\n";
    }
```

## MULTI interface

### Simple to use.

[multi-single.c](https://curl.haxx.se/libcurl/c/multi-single.html) above is roughly rewritten as follows.

```cpp
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include "uccurl.h"

int main()
{
    try {
        uc::curl::global libcurlInit;

        uc::curl::easy http_handle("http://www.example.com/");

        uc::curl::multi multi_handle;
        multi_handle.add(http_handle);

        while (multi_handle.perform() > 0) {
            if (multi_handle.wait(std::chrono::seconds(1)) == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (std::exception& ex) {
        std::cerr << "exception : " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### With `select()`

```c++
    // See https://curl.haxx.se/libcurl/c/multi-app.html
    uc::curl::fdsets sets;
    
    while (multi_handle.perform() > 0) {
        multi_handle.fdset(sets);
        auto timeout = std::min(multi_handle.timeout(), std::chrono::milliseconds(1000));
        if (!sets) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } else if (sets.select(timeout) == -1) {
            break;
        }
        sets.zero();
    }
```

### `curl_multi_info_read()` API

Instead of `curl_multi_info_read()`, there are `for_each_done_info()`.


```cpp
// before
while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
        char *url;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &url);
        printf("%d : %s\n", msg->data.result, url);
    }
}

// after
multi_handle.for_each_done_info([](uc::curl::easy_ref&& h, CURLcode result) {
    std::cout << result <<  " : " << h.uri() << "\n";
});
```




