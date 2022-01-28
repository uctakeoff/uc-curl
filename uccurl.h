/**
uc::curl 

Copyright (c) 2017-2021, Kentaro Ushiyama

This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php
*/
#ifndef UC_CURL_H
#define UC_CURL_H
#define UC_CURL_VERSION "0.5.0"
#define UC_CURL_VERSION_NUM 0x000500

#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <system_error>
#include <functional>
#include <chrono>
#include <curl/curl.h>

namespace uc {
namespace curl {

//-----------------------------------------------------------------------------
// type traits, smart pointer

namespace detail
{
    template<bool B> using sfinae = typename std::enable_if<B, std::nullptr_t>::type;
    template <typename T> using decay_t = typename std::decay<T>::type;
    template <typename T, typename U> using is_same_decay = std::is_same<decay_t<T>, decay_t<U>>;

    // dummy. only use as a pointer.
    struct easy_handle {};
    struct multi_handle {};
    struct share_handle {};

    template <typename T> struct traits;
    template <> struct traits<char>          { static constexpr decltype(&curl_free) cleanup = curl_free; };
    template <> struct traits<easy_handle>   { static constexpr decltype(&curl_easy_cleanup) cleanup = curl_easy_cleanup; };
    template <> struct traits<multi_handle>  { static constexpr decltype(&curl_multi_cleanup) cleanup = curl_multi_cleanup; };
    template <> struct traits<share_handle>  { static constexpr decltype(&curl_share_cleanup) cleanup = curl_share_cleanup; };
    template <> struct traits<curl_slist>    { static constexpr decltype(&curl_slist_free_all) cleanup  = curl_slist_free_all; };
    template <> struct traits<curl_httppost> { static constexpr decltype(&curl_formfree) cleanup  = curl_formfree; };
    template <typename T> struct deleter
    {
        void operator()(T* handle) const noexcept
        {
            traits<T>::cleanup(handle);
        }
    };
 
#if __cpp_lib_nonmember_container_access >= 201411
    template <typename C> constexpr auto data(C&& c) { return std::data(c); }
    template <typename T, typename C, decltype(std::declval<C>().size()) = 0> constexpr T size(const C& c) { return static_cast<T>(std::size(c)); }
#else
    template <typename C> constexpr auto data(C& c) -> decltype(c.data()) { return c.data(); }
    template <typename C> constexpr auto data(const C& c) -> decltype(c.data()) { return c.data(); }
    template <typename A, std::size_t N> constexpr A* data(A (&array)[N]) noexcept { return array; }
    template <typename E> constexpr const E* data(std::initializer_list<E> il) noexcept { return il.begin(); }
    template <typename T, typename C, decltype(std::declval<C>().size()) = 0> constexpr T size(const C& c) { return static_cast<T>(c.size()); }
    template <typename T, typename A, std::size_t N> constexpr T size(const A (&array)[N]) noexcept { return static_cast<T>(N); }
#endif
}
template <typename T> using safe_ptr = std::unique_ptr<T, detail::deleter<T>>;
template <typename Handle> class basic_easy;

//-----------------------------------------------------------------------------
// error , exception

inline const char* strerror(CURLcode errcode) noexcept {return curl_easy_strerror(errcode);}
inline const char* strerror(CURLMcode errcode) noexcept {return curl_multi_strerror(errcode);}
inline const char* strerror(CURLSHcode errcode) noexcept {return curl_share_strerror(errcode);}
inline std::string strerror(CURLFORMcode errcode) {return std::string("CURLFORMcode(").append(std::to_string(errcode)).append(")");}

template<typename E> class error_category : public std::error_category
{
public:
    static const std::error_category& get_instance() noexcept
    {
        static error_category<E> cat;
        return cat;
    }
    const char* name() const noexcept override
    {
        return "uc::curl";
    }
    std::string message(int ec) const override
    {
        return std::string{strerror(static_cast<E>(ec))};
    }
};

#define UC_CURL_ASSERT(cmd, err) if (!(cmd)) {throw std::system_error{err, error_category<decltype(err)>::get_instance(), std::string{"uc::curl::"}.append(__func__)};}
#define UC_CURL_ASSERT_CURLCODE(cmd)     {CURLcode err = (cmd); UC_CURL_ASSERT(err == CURLE_OK, err);}
#define UC_CURL_ASSERT_CURLMCODE(cmd)    {CURLMcode err = (cmd); UC_CURL_ASSERT((err == CURLM_OK) || (err == CURLM_CALL_MULTI_PERFORM), err);}
#define UC_CURL_ASSERT_CURLSHCODE(cmd)   {CURLSHcode err = (cmd); UC_CURL_ASSERT(err == CURLSHE_OK, err);}
#define UC_CURL_ASSERT_CURLFORMCODE(cmd) {CURLFORMcode err = (cmd); UC_CURL_ASSERT(err == CURL_FORMADD_OK, err);}

//-----------------------------------------------------------------------------
// startup , cleanup 

class global
{
public:
    explicit global(long flags = CURL_GLOBAL_ALL)
    {
        UC_CURL_ASSERT_CURLCODE(curl_global_init(flags));
    }
    global(curl_malloc_callback m, curl_free_callback f, curl_realloc_callback r, curl_strdup_callback s, curl_calloc_callback c)
    {
        UC_CURL_ASSERT_CURLCODE(curl_global_init_mem(CURL_GLOBAL_ALL, m, f, r, s, c));
    }
    global(long flags, curl_malloc_callback m, curl_free_callback f, curl_realloc_callback r, curl_strdup_callback s, curl_calloc_callback c)
    {
        UC_CURL_ASSERT_CURLCODE(curl_global_init_mem(flags, m, f, r, s, c));
    }
    ~global() noexcept
    {
        curl_global_cleanup();
    }
};

//-----------------------------------------------------------------------------
// utilities

inline const char* version() noexcept
{
    return ::curl_version();
}
inline const curl_version_info_data& version_info(CURLversion type = CURLVERSION_NOW)
{
    curl_version_info_data* ret = curl_version_info(type);
    UC_CURL_ASSERT(ret, CURLE_FAILED_INIT);
    return *ret;
}
inline time_t getdate(const char* p, const time_t* unused = nullptr) noexcept
{
    return curl_getdate(p, unused);
}
inline time_t getdate(const std::string& p, const time_t* unused = nullptr) noexcept
{
    return curl_getdate(p.c_str(), unused);
}
inline std::string escape(const std::string& str)
{
    return std::string{safe_ptr<char>(curl_escape(str.c_str(), static_cast<int>(str.size()))).get()};
}
inline std::string unescape(const std::string& str)
{
    return std::string{safe_ptr<char>(curl_unescape(str.c_str(), static_cast<int>(str.size()))).get()};
}

//-----------------------------------------------------------------------------
// callbacks

namespace detail
{
    constexpr std::ios::seekdir to_seekdir(int origin) noexcept
    {
        return (origin == SEEK_CUR) ? std::ios::cur
            :  (origin == SEEK_END) ? std::ios::end
            :  std::ios::beg;
    }

    template <typename T, sfinae<is_same_decay<std::string, T>::value> = nullptr > 
    size_t write(T& str, const char* ptr, size_t nbytes)
    {
        str.append(ptr, nbytes);
        return nbytes;
    }
    template <typename T, sfinae<std::is_base_of<std::ostream, T>::value> = nullptr > 
    size_t write(T& os, const char* ptr, size_t nbytes)
    {
        const auto pos = os.tellp();
        if (!os.fail() && (pos == -1)) {    // tellp() is not supported.
            os.write(ptr, nbytes);
            return nbytes;
        } else {
            return static_cast<size_t>(os.write(ptr, nbytes).tellp() - pos);
        }
    }
    template <typename T, sfinae<!std::is_base_of<std::ostream, T>::value && !is_same_decay<std::string, T>::value> = nullptr> 
    size_t write(T& func, const char* ptr, size_t nbytes)
    {
        return func(ptr, nbytes);
    }

    template <typename T, sfinae<std::is_base_of<std::istream, T>::value> = nullptr> 
    size_t read(T& is, char* ptr, size_t nbytes)
    {
        return static_cast<size_t>(is.read(ptr, nbytes).gcount());
    }
    template <typename T, sfinae<!std::is_base_of<std::istream, T>::value> = nullptr> 
    size_t read(T& func, char* ptr, size_t nbytes)
    {
        return func(ptr, nbytes);
    }

    template <typename T, sfinae<std::is_base_of<std::istream, T>::value> = nullptr> 
    bool seek(T& is, curl_off_t offset, int origin)
    {
        return !is.seekg(offset, to_seekdir(origin)).fail();
    }
    template <typename T, sfinae<!std::is_base_of<std::istream, T>::value>*& = nullptr> 
    bool seek(T& is, curl_off_t offset, int origin)
    {
        return false;
    }

    template <typename T> size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userp) noexcept
    {
        try {
            return write<T>(*static_cast<T*>(userp), static_cast<const char*>(ptr), size * nmemb);
        } catch (...) {}
        return 0;
    }
    template <typename T> size_t read_cb(char* ptr, size_t size, size_t nmemb, void* userp) noexcept
    {
        try {
            return read<T>(*static_cast<T*>(userp), ptr, size * nmemb);
        } catch (...) {}
        return CURL_READFUNC_ABORT;
    }
    template <typename T> int seek_cb(void* userp, curl_off_t offset, int origin) noexcept
    {
        try {
            return seek<T>(*static_cast<T*>(userp), offset, origin) ? CURL_SEEKFUNC_OK : CURL_SEEKFUNC_CANTSEEK;
        } catch (...) {}
        return CURL_SEEKFUNC_FAIL;
    }
}

//-----------------------------------------------------------------------------
// share interface

class share
{
public:
    using handle_type = safe_ptr<detail::share_handle>;
    using pointer = CURLSH*;

    template <typename Mutex> static void lock(CURL*, curl_lock_data, curl_lock_access, void *userp)
    {
        static_cast<Mutex*>(userp)->lock();
    }
    template <typename Mutex> static void unlock(CURL*, curl_lock_data, void* userp)
    {
        static_cast<Mutex*>(userp)->unlock();
    }

    share() : handle(static_cast<detail::share_handle*>(::curl_share_init()))
    {
        UC_CURL_ASSERT(handle, CURLE_FAILED_INIT);
    }
    share(const share&) = delete;
    share(share&& obj) noexcept = default;
    share& operator=(const share&) = delete;
    share& operator=(share&& obj) noexcept = default;
    ~share() noexcept = default;

    explicit operator bool() const noexcept { return static_cast<bool>(handle); }
    pointer native_handle() const noexcept { return handle.get(); }
    void swap(share& obj) noexcept { handle.swap(obj.handle); }

    share& set(curl_lock_data data)
    {
        return setopt(CURLSHOPT_SHARE, data);
    }
    share& clear(curl_lock_data data)
    {
        return setopt(CURLSHOPT_UNSHARE, data);
    }
    // Mutex is like 'std::mutex'
    template <typename Mutex> share& set_mutex(Mutex& mutex)
    {
        return setopt(CURLSHOPT_USERDATA, &mutex).setopt(CURLSHOPT_LOCKFUNC, lock<Mutex>).setopt(CURLSHOPT_UNLOCKFUNC, unlock<Mutex>);
    }

    template <typename T> share& setopt(CURLSHoption option, T parameter)
    {
        UC_CURL_ASSERT_CURLSHCODE(curl_share_setopt(handle.get(), option, parameter));
        return *this;
    }
private:
    handle_type handle;
};
inline void swap(share& a, share& b) noexcept
{
    a.swap(b);
}

//-----------------------------------------------------------------------------
// curl_slist API

inline curl_slist* append(curl_slist* list, const char* str) noexcept
{
    return curl_slist_append(list, str);
}
inline curl_slist* append(curl_slist* list, const std::string& str) noexcept
{
    return append(list, str.c_str());
}
template <typename T1, typename T2, typename... Ts> curl_slist* append(curl_slist* list, T1&& str1, T2&& str2, Ts&&... strs) noexcept
{
    return append(append(list, str1), std::forward<T2>(str2), std::forward<Ts>(strs)...);
}
//! F : void (const char*)
template <typename F> void for_in_slist(const curl_slist* list, F func)
{
    for (const curl_slist* i = list; i; i = i->next) {
        func(i->data);
    }
}

using slist = safe_ptr<curl_slist>;

template <typename... Ts> void append(slist& list, Ts&&... strs) noexcept
{
    list.reset(append(list.release(), std::forward<Ts>(strs)...));
}
template <typename... Ts> slist create_slist(Ts&&... strs) noexcept
{
    return slist(append(nullptr, std::forward<Ts>(strs)...));
}
class slist_iterator : public std::iterator<std::input_iterator_tag, char*, void, char**, char*&>
{
public:
    typedef char* value_type;

    explicit slist_iterator(const curl_slist* p = nullptr) : ptr{p}
    {
    }
    const value_type& operator *() const noexcept
    {
        return ptr->data;
    }
    slist_iterator& operator ++() noexcept
    {
        ptr = ptr->next;
        return *this;
    }
    const slist_iterator operator++(int) noexcept
    {
        slist_iterator itr(*this);
        operator++();
        return itr;
    }
    friend bool operator==(const slist_iterator& a, const slist_iterator& b) noexcept
    {
        return a.ptr == b.ptr;
    }
    friend bool operator!=(const slist_iterator& a, const slist_iterator& b) noexcept
    {
        return !operator==(a, b);
    }
private:
    const curl_slist* ptr{};
};
namespace detail
{
    inline slist_iterator begin(const slist& l)
    {
        return slist_iterator(l.get());
    }
    inline slist_iterator end(const slist& l)
    {
        return slist_iterator(nullptr);
    }
}

//-----------------------------------------------------------------------------
// form API

class form
{
public:
    using handle_type = safe_ptr<curl_httppost>;
    using pointer = typename handle_type::pointer;

    form() = default;
    form(const form&) = delete;
    form(form&& obj) noexcept = default;
    ~form() noexcept = default;
    form& operator=(const form&) = delete;
    form& operator=(form&& obj) noexcept = default;

    explicit operator bool() const noexcept { return static_cast<bool>(first); }
    pointer native_handle() const noexcept { return first.get(); }


    form& forms(const std::string& name, const curl_forms* list)
    {
        return add(name, CURLFORM_ARRAY, list);
    }

    template <typename Str> form& contents(const Str& name, const char* conts)
    {
        return add(name, CURLFORM_PTRCONTENTS, conts);
    }
    template <typename Str> form& contents(const Str& name, const char* conts, const std::string& contenttype)
    {
        return add(name, CURLFORM_PTRCONTENTS, conts, CURLFORM_CONTENTTYPE, contenttype.c_str());
    }
    template <typename Str, typename Container> form& contents(const Str& name, const Container& conts)
    {
        return add(name, CURLFORM_PTRCONTENTS, detail::data(conts), CURLFORM_CONTENTSLENGTH, detail::size<long>(conts));
    }
    template <typename Str, typename Container> form& contents(const Str& name, const Container& conts, const std::string& contenttype)
    {
        return add(name, CURLFORM_PTRCONTENTS, detail::data(conts), CURLFORM_CONTENTSLENGTH, detail::size<long>(conts),
                CURLFORM_CONTENTTYPE, contenttype.c_str());
    }
    template <typename Str, typename Container> form& copy_contents(const Str& name, Container&& conts)
    {
        return add(name, CURLFORM_COPYCONTENTS, detail::data(conts), CURLFORM_CONTENTSLENGTH, detail::size<long>(conts));
    }
    template <typename Str, typename Container> form& copy_contents(const Str& name, Container&& conts, const std::string& contenttype)
    {
        return add(name, CURLFORM_COPYCONTENTS, detail::data(conts), CURLFORM_CONTENTSLENGTH, detail::size<long>(conts),
                CURLFORM_CONTENTTYPE, contenttype.c_str());
    }

    form& file(const std::string& name, const std::string& filename)
    {
        return add(name, CURLFORM_FILE, filename.c_str());
    }
    form& file(const std::string& name, const std::string& filename, const std::string& contenttype)
    {
        return add(name, CURLFORM_FILE, filename.c_str(), CURLFORM_CONTENTTYPE, contenttype.c_str());
    }

    template <typename Container> form& buffer(const std::string& name, const std::string& filename, const Container& data)
    {
        return add(name, CURLFORM_BUFFER, filename.c_str(), CURLFORM_BUFFERPTR, detail::data(data), 
                CURLFORM_BUFFERLENGTH, detail::size<long>(data));
    }
    template < typename Container > form& buffer(const std::string& name, const std::string& filename, const Container& data, const std::string& contenttype)
    {
        return add(name, CURLFORM_BUFFER, filename.c_str(), CURLFORM_BUFFERPTR, detail::data(data), 
                CURLFORM_BUFFERLENGTH, detail::size<long>(data), CURLFORM_CONTENTTYPE, contenttype.c_str());
    }

    template < typename... Args > form& add(const char* name, CURLformoption option, Args&&... args)
    {
        return add(CURLFORM_COPYNAME, name, option, std::forward<Args>(args)...);
    }
    template < typename... Args > form& add(const std::string& name, CURLformoption option, Args&&... args)
    {
        return add(CURLFORM_COPYNAME, name.c_str(), CURLFORM_NAMELENGTH, name.size(), option, std::forward<Args>(args)...);
    }
    template < typename... Args > form& add(CURLformoption option, Args... args)
    {
        auto f = first.get();
        UC_CURL_ASSERT_CURLFORMCODE(::curl_formadd(&f, &last, option, args..., CURLFORM_END));
        first.release();
        first.reset(f);
        return *this;
    }

    template <typename T> T serialize() const
    {
        T ret {};
        serialize<T>(ret);
        return ret;
    }
    template <typename T> bool serialize(T& output) const
    {
        return get(&output, &formget_cb<T>);
    }
    bool get(void* userp, curl_formget_callback append) const
    {
        return curl_formget(first.get(), userp, append) == 0;
    }

private:
    template <typename T> static size_t formget_cb(void* userp, const char* buf, size_t len)
    {
        try {
            return detail::write<T>(*static_cast<T*>(userp), buf, len);
        } catch (...) {}
        return 0;
    }
    safe_ptr<curl_httppost> first;
    curl_httppost* last = nullptr;
};

#if LIBCURL_VERSION_NUM >= 0x073800
//-----------------------------------------------------------------------------
// mime API

namespace detail
{
    template <> struct traits<curl_mime>   { static constexpr decltype(&curl_mime_free) cleanup = curl_mime_free; };
}

class mime_part;

class mime
{
    friend class mime_part;
public:
    using handle_type = safe_ptr<curl_mime>;
    using pointer = typename handle_type::pointer;

    mime() = default;
    mime(CURL* easy) noexcept : handle{curl_mime_init(easy)} {}
    template<typename T> mime(basic_easy<T>& easy) noexcept;

    explicit operator bool() const noexcept { return static_cast<bool>(handle); }
    pointer native_handle() const noexcept { return &(*handle); }
    void swap(mime& obj) { std::swap(handle, obj.handle); }

    mime_part addpart();
private:
    handle_type handle{};
};

class mime_part
{
public:
    mime_part() = default;
    mime_part(curl_mimepart* p) : handle{p} {}

    explicit operator bool() const noexcept { return static_cast<bool>(handle); }
    curl_mimepart* native_handle() const noexcept { return handle; }

    mime_part& name(const std::string& value) { return name(value.c_str()); }
    mime_part& name(const char* value)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_name(handle, value));
        return *this;
    }

    mime_part& filename(const std::string& value) { return filename(value.c_str()); }
    mime_part& filename(const char* value)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_filename(handle, value));
        return *this;
    }

    mime_part& type(const std::string& value) { return type(value.c_str()); }
    mime_part& type(const char* value)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_type(handle, value));
        return *this;
    }

    mime_part& encoder(const std::string& value) { return encoder(value.c_str()); }
    mime_part& encoder(const char* value)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_encoder(handle, value));
        return *this;
    }

    mime_part& filedata(const std::string& filename) { return filedata(filename.c_str()); }
    mime_part& filedata(const char* filename)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_filedata(handle, filename));
        return *this;
    }

    mime_part& data(const std::string& str) { return data(str.c_str(), str.size()); }
    mime_part& data(const char* data, size_t size = CURL_ZERO_TERMINATED)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_data(handle, data, size));
        return *this;
    }

    mime_part& data(std::istream& is)
    {
        const auto nbytes = static_cast<curl_off_t>(is.seekg(0, is.end).tellg());
        return data(is.seekg(0, is.beg), nbytes);
    }
    mime_part& data(std::istream& is, curl_off_t nbytes)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_data_cb(handle, nbytes, &detail::read_cb<std::istream>, &detail::seek_cb<std::istream>, nullptr, &is));
        return *this;
    }
    mime_part& subparts(mime&& subparts)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_subparts(handle, subparts.native_handle()));
        subparts.handle.release();
        return *this;
    }
    mime_part& headers(const curl_slist* header_list, int take_ownership)
    {
        UC_CURL_ASSERT_CURLCODE(curl_mime_headers(handle, const_cast<curl_slist*>(header_list), take_ownership));
        return *this;
    }
    mime_part& headers(const slist& header_list) { return headers(header_list.get(), 0); }
    mime_part& headers(slist&& header_list) { return headers(header_list.release(), 1); }

private:
    curl_mimepart* handle{};
};

inline mime_part mime::addpart()
{
    return mime_part{curl_mime_addpart(handle.get())};
}
#endif

namespace detail
{
    //-----------------------------------------------------------------------------
    // CURLINFO policy

    template < int InfoType > struct infogroup    {using type = void*;};
    template <> struct infogroup<CURLINFO_STRING> {using type = const char*;};
    template <> struct infogroup<CURLINFO_LONG>   {using type = long;};
    template <> struct infogroup<CURLINFO_DOUBLE> {using type = double;};
    template <> struct infogroup<CURLINFO_SLIST>  {using type = curl_slist*;};
#if LIBCURL_VERSION_NUM >= 0x072d00
    template <> struct infogroup<CURLINFO_SOCKET> {using type = curl_socket_t;};
#endif
#if LIBCURL_VERSION_NUM >= 0x073700
    template <> struct infogroup<CURLINFO_OFF_T>  {using type = curl_off_t;};
#endif

    template < CURLINFO Info > struct info        {using type = typename infogroup<Info & CURLINFO_TYPEMASK>::type;};
    template <> struct info<CURLINFO_PRIVATE>     {using type = void*;};
#if LIBCURL_VERSION_NUM >= 0x071301
    template <> struct info<CURLINFO_CERTINFO>    {using type = curl_certinfo*;};
#endif
#if LIBCURL_VERSION_NUM >= 0x072200
    template <> struct info<CURLINFO_TLS_SESSION> {using type = curl_tlssessioninfo*;};
#endif
#if LIBCURL_VERSION_NUM >= 0x073000
    template <> struct info<CURLINFO_TLS_SSL_PTR> {using type = curl_tlssessioninfo*;};
#endif

    template <typename T> struct geti           {using type = T;};
    template <> struct geti<curl_slist*>        {using type = slist;};

    template < CURLINFO Info > using info_t = typename detail::info<Info>::type;
    template < CURLINFO Info > using get_t  = typename detail::geti<info_t<Info>>::type;

    //-----------------------------------------------------------------------------
    // CURLOPT policy , setopt()

    template <int OptionType> constexpr bool is_type(CURLoption opt) noexcept {return (OptionType <= opt) && (opt < OptionType + 10000); }
    constexpr bool is_long(CURLoption opt) noexcept {return is_type<CURLOPTTYPE_LONG>(opt); }
    constexpr bool is_objptr(CURLoption opt) noexcept {return is_type<CURLOPTTYPE_OBJECTPOINT>(opt); }
    constexpr bool is_funcptr(CURLoption opt) noexcept {return is_type<CURLOPTTYPE_FUNCTIONPOINT>(opt); }
    constexpr bool is_off_t(CURLoption opt)  noexcept {return is_type<CURLOPTTYPE_OFF_T>(opt); }

    // type safe setopt()
    template <CURLoption Option, sfinae<is_long(Option)> = nullptr>
    CURLcode setopt(CURL* handle, long parameter) noexcept
    {
        return curl_easy_setopt(handle, Option, parameter);
    }
    template <CURLoption Option, typename T, sfinae<is_objptr(Option)> = nullptr, sfinae<std::is_pointer<decay_t<T>>::value> = nullptr>
    CURLcode setopt(CURL* handle, T&& parameter) noexcept
    {
        return curl_easy_setopt(handle, Option, parameter);
    }
    template <CURLoption Option, typename T, sfinae<is_objptr(Option)> = nullptr, sfinae<is_same_decay<T, std::nullptr_t>::value> = nullptr>
    CURLcode setopt(CURL* handle, T&&) noexcept
    {
        return curl_easy_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLoption Option, typename T, sfinae<is_objptr(Option)> = nullptr, sfinae<is_same_decay<T, std::string>::value> = nullptr>
    CURLcode setopt(CURL* handle, T&& str) noexcept
    {
        return curl_easy_setopt(handle, Option, str.c_str());
    }
    template <CURLoption Option, typename T, sfinae<is_objptr(Option)> = nullptr, sfinae<is_same_decay<T, share>::value> = nullptr>
    CURLcode setopt(CURL* handle, const T& share)
    {
        return curl_easy_setopt(handle, Option, share.native_handle());
    }
    template <CURLoption Option, typename T, sfinae<is_objptr(Option)> = nullptr, sfinae<is_same_decay<T, slist>::value> = nullptr>
    CURLcode setopt(CURL* handle, const T& slist)
    {
        return curl_easy_setopt(handle, Option, slist.get());
    }
    template <CURLoption Option, typename T, sfinae<is_objptr(Option)> = nullptr, sfinae<is_same_decay<T, form>::value> = nullptr>
    CURLcode setopt(CURL* handle, const T& form)
    {
        return curl_easy_setopt(handle, Option, form.native_handle());
    }
#if LIBCURL_VERSION_NUM >= 0x073800
    template <CURLoption Option, typename T, sfinae<is_objptr(Option)> = nullptr, sfinae<is_same_decay<T, mime>::value> = nullptr>
    CURLcode setopt(CURL* handle, const T& mime)
    {
        return curl_easy_setopt(handle, Option, mime.native_handle());
    }
#endif
    template <CURLoption Option, typename T, sfinae<is_funcptr(Option)> = nullptr, sfinae<std::is_function<typename std::remove_pointer<T>::type>::value> = nullptr>
    CURLcode setopt(CURL* handle, T parameter) noexcept
    {
        return curl_easy_setopt(handle, Option, parameter);
    }
    template <CURLoption Option, typename T, sfinae<is_funcptr(Option)> = nullptr, sfinae<is_same_decay<T, std::nullptr_t>::value> = nullptr>
    CURLcode setopt(CURL* handle, T&&) noexcept
    {
        return curl_easy_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLoption Option, sfinae<is_off_t(Option)> = nullptr>
    CURLcode setopt(CURL* handle, curl_off_t parameter) noexcept
    {
        return curl_easy_setopt(handle, Option, parameter);
    }

    // type safe clear function
    template <CURLoption Option, sfinae<is_long(Option)> = nullptr>
    CURLcode clearopt(CURL* handle) noexcept
    {
        return curl_easy_setopt(handle, Option, 0L);
    }
    template <CURLoption Option, sfinae<is_objptr(Option)> = nullptr>
    CURLcode clearopt(CURL* handle) noexcept
    {
        return curl_easy_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLoption Option, sfinae<is_funcptr(Option)> = nullptr>
    CURLcode clearopt(CURL* handle) noexcept
    {
        return curl_easy_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLoption Option, sfinae<is_off_t(Option)> = nullptr>
    CURLcode clearopt(CURL* handle) noexcept
    {
        return curl_easy_setopt(handle, Option, static_cast<curl_off_t>(0));
    }
}

//-----------------------------------------------------------------------------
// easy interface

template <typename Handle> class basic_easy;

// "uc::curl::easy" calls curl_easy_cleanup in the destructor. 
using easy = basic_easy<safe_ptr<detail::easy_handle>>;

// "uc::curl::easy_ref" does not release resources. It is for reference only.
using easy_ref = basic_easy<detail::easy_handle*>;


template <typename Handle> class basic_easy
{
public:
    static constexpr size_t DEFAULT_MAX_REDIRECTS = 20;
    using handle_type = Handle;
    using pointer = CURL*;

    // easy only.
    basic_easy();
    explicit basic_easy(const char* serverURI, long maxRedirects = DEFAULT_MAX_REDIRECTS);
    explicit basic_easy(const std::string& serverURI, long maxRedirects = DEFAULT_MAX_REDIRECTS);

    // easy_ref only
    basic_easy(CURL*);

    // common
    ~basic_easy() noexcept = default;
    basic_easy(basic_easy&& obj) noexcept = default;
    basic_easy& operator=(basic_easy&& obj) noexcept = default;
    basic_easy(const basic_easy& obj);
    basic_easy& operator=(const basic_easy& obj);

    explicit operator bool() const noexcept { return static_cast<bool>(handle); }
    pointer native_handle() const noexcept { return &(*handle); }
    void swap(basic_easy& obj) noexcept { handle.swap(obj.handle); }

    const char* uri() const
    {
        return getinfo<CURLINFO_EFFECTIVE_URL>();
    }
    basic_easy& uri(const std::string& serverURI)
    {
        return setopt(CURLOPT_URL, serverURI.c_str());
    }
    // count == -1 : unlimited
    basic_easy& max_redirects(long count)
    {
        return setopt(CURLOPT_FOLLOWLOCATION, count == 0 ? 1L : 0L).setopt(CURLOPT_MAXREDIRS, count);
    }

    // See https://curl.haxx.se/libcurl/c/CURLOPT_HEADEROPT.html
    basic_easy& header(const slist& headers)
    {
        return setopt(CURLOPT_HEADEROPT, CURLHEADER_SEPARATE).setopt(CURLOPT_HTTPHEADER, headers.get());
    }
#if LIBCURL_VERSION_NUM >= 0x072500
    basic_easy& header(const slist& headers, const slist& proxyHeaders)
    {
        return header(headers).setopt(CURLOPT_PROXYHEADER, proxyHeaders.get());
    }
#endif

    basic_easy& postfields(const void* data, curl_off_t nbytes)
    {
        return setopt(CURLOPT_POSTFIELDSIZE_LARGE, nbytes).setopt(CURLOPT_POSTFIELDS, data);
    }
    basic_easy& copy_postfields(const void* data, curl_off_t nbytes)
    {
        return setopt(CURLOPT_POSTFIELDSIZE_LARGE, nbytes).setopt(CURLOPT_COPYPOSTFIELDS, data);
    }
    basic_easy& postfields(const std::string& str)
    {
        return postfields(str.c_str(), str.size());
    }
    basic_easy& postfields(std::string&& str)
    {
        return copy_postfields(str.c_str(), str.size());
    }
    basic_easy& postfields(std::istream& is)
    {
        const auto nbytes = static_cast<curl_off_t>(is.seekg(0, is.end).tellg());
        return postfields(nullptr, nbytes).body(is.seekg(0, is.beg), nbytes);
    }
    basic_easy& postfields(const form& forms)
    {
        return setopt<CURLOPT_HTTPPOST>(forms);
    }

    basic_easy& body(std::istream& is)
    {
        const auto nbytes = static_cast<curl_off_t>(is.seekg(0, is.end).tellg());
        return body(is.seekg(0, is.beg), nbytes);
    }
    basic_easy& body(std::istream& is, curl_off_t nbytes)
    {
        setopt(CURLOPT_INFILESIZE_LARGE, nbytes);
        setopt(CURLOPT_READDATA, &is).setopt(CURLOPT_READFUNCTION, &detail::read_cb<std::istream>);
        setopt(CURLOPT_SEEKDATA, &is).setopt(CURLOPT_SEEKFUNCTION, &detail::seek_cb<std::istream>);
        return *this;
    }

    // set response callback.  T : std::string, std::ostream, std::function<size_t(const char*, size_t)>
    template <typename T> basic_easy& response(T& output)
    {
        return setopt(CURLOPT_WRITEDATA, &output).setopt(CURLOPT_WRITEFUNCTION, &detail::write_cb<T>);
    }
    // clear response callback.
    basic_easy& response()
    {
        clear<CURLOPT_WRITEDATA>();
        clear<CURLOPT_WRITEFUNCTION>();
        return *this;
    }
    // set response header callback.  T : std::string, std::ostream, std::function<size_t(const char*, size_t)>
    template <typename T> basic_easy& response_header(T& output)
    {
        return setopt(CURLOPT_WRITEHEADER, &output).setopt(CURLOPT_HEADERFUNCTION, &detail::write_cb<T>);
    }
    // clear response header callback.
    basic_easy& response_header()
    {
        clear<CURLOPT_WRITEHEADER>();
        clear<CURLOPT_HEADERFUNCTION>();
        return *this;
    }
    
    basic_easy& share(share& curlsh)
    {
        return setopt(CURLOPT_SHARE, curlsh.native_handle());
    }

    template <typename T> basic_easy& private_data(T* obj)
    {
        return setopt(CURLOPT_PRIVATE, obj);
    }
    template <typename T> T* private_data() const
    {
        return getinfo<T*>(CURLINFO_PRIVATE);
    }

    basic_easy& progress(curl_progress_callback callback, void* data)
    {
        return setopt(CURLOPT_NOPROGRESS, 0L).setopt(CURLOPT_PROGRESSFUNCTION, callback).setopt(CURLOPT_PROGRESSDATA, data);
    }
#if LIBCURL_VERSION_NUM >= 0x072000
    basic_easy& progress(curl_xferinfo_callback callback, void* data)
    {
        return setopt(CURLOPT_NOPROGRESS, 0L).setopt(CURLOPT_XFERINFOFUNCTION, callback).setopt(CURLOPT_XFERINFODATA, data);
    }
#endif

    void perform()
    {
        UC_CURL_ASSERT_CURLCODE(curl_easy_perform(native_handle()));
    }
    void reset() noexcept
    {
        curl_easy_reset(native_handle());
    }
    void pause(int bitmask)
    {
        UC_CURL_ASSERT_CURLCODE(curl_easy_pause(native_handle(), bitmask));
    }

    template <CURLoption Option> basic_easy& setopt()
    {
        UC_CURL_ASSERT_CURLCODE(detail::setopt<Option>(native_handle(), 1L));
        return *this;
    }
    template <CURLoption Option> basic_easy& clear()
    {
        UC_CURL_ASSERT_CURLCODE(detail::clearopt<Option>(native_handle()));
        return *this;
    }
    template <CURLoption Option, typename T> basic_easy& setopt(T&& parameter)
    {
        UC_CURL_ASSERT_CURLCODE(detail::setopt<Option>(native_handle(), std::forward<T>(parameter)));
        return *this;
    }
    template <typename T> basic_easy& setopt(CURLoption option, const T& parameter)
    {
       UC_CURL_ASSERT_CURLCODE(curl_easy_setopt(native_handle(), option, parameter));
        return *this;
    }

    //! Request internal information from the curl session with this function.
    template <typename T, typename U = T > T getinfo(CURLINFO info) const
    {
        U ret {};
        UC_CURL_ASSERT_CURLCODE(curl_easy_getinfo(native_handle(), info, &ret));
        return T{ret};
    }
    template <CURLINFO Info> detail::get_t<Info> getinfo() const
    {
        return getinfo<detail::get_t<Info>, detail::info_t<Info>>(Info);
    }

    safe_ptr<char> escape(const char* str, int len) const
    {
        return safe_ptr<char>(curl_easy_escape(native_handle(), str, len));
    }
    std::string escape(const std::string& str) const
    {
        auto s = escape(str.c_str(), str.size());
        return s ? std::string{s.get()} : std::string{};
    }
    safe_ptr<char> unescape(const char* str, int len = 0, int* outputlen = nullptr) const
    {
        return safe_ptr<char>(curl_easy_unescape(native_handle(), str, len, outputlen));
    }
    std::string unescape(const std::string& str) const
    {
        int outputlen = 0;
        auto s = unescape(str.c_str(), str.size(), &outputlen);
        return s ? std::string{s.get(), outputlen} : std::string{};
    }

    curl_socket_t get_socket() const
    {
#if LIBCURL_VERSION_NUM >= 0x072d00
        return getinfo<CURLINFO_ACTIVESOCKET>();
#else
        return getinfo<CURLINFO_LASTSOCKET>();
#endif
    }
    CURLcode recv(void* buffer, size_t buflen, size_t* nbytes)
    {
        return curl_easy_recv(native_handle(), buffer, buflen, nbytes);
    }
    CURLcode send(const void* buffer, size_t buflen, size_t* nbytes)
    {
        return curl_easy_send(native_handle(), buffer, buflen, nbytes);
    }

private:
    handle_type handle;
};

namespace detail
{
    inline easy::handle_type new_handle()
    {
        auto h = ::curl_easy_init();
        UC_CURL_ASSERT(h, CURLE_FAILED_INIT);
        return easy::handle_type(static_cast<detail::easy_handle*>(h));
    }
    inline easy::handle_type dup_handle(const easy::handle_type& handle)
    {
        auto h = ::curl_easy_duphandle(handle.get());
        UC_CURL_ASSERT(h, CURLE_FAILED_INIT);
        return easy::handle_type(static_cast<detail::easy_handle*>(h));
    }
}
template<> inline easy::basic_easy() : handle(detail::new_handle())
{
}
template<> inline easy::basic_easy(const easy& obj) : handle(detail::dup_handle(obj.handle))
{
}
template<> inline easy::basic_easy(const char* serverURI, long maxRedirects) : handle(detail::new_handle())
{
    uri(serverURI).max_redirects(maxRedirects);
}
template<> inline easy::basic_easy(const std::string& serverURI, long maxRedirects) : handle(detail::new_handle())
{
    uri(serverURI).max_redirects(maxRedirects);
}
template<> inline easy& easy::operator=(const easy& obj)
{
    if (&obj != this) {
        handle = detail::dup_handle(obj.handle);
    }
    return *this;
}

template<> inline easy_ref::basic_easy(CURL* handle) : handle(static_cast<detail::easy_handle*>(handle))
{
}
template<> inline easy_ref::basic_easy(const easy_ref& obj) : handle(obj.handle)
{
}
template<> inline easy_ref& easy_ref::operator=(const easy_ref& obj)
{
    handle = obj.handle;
    return *this;
}

template <typename T, typename U> bool operator==(const basic_easy<T>& a, const basic_easy<U>& b) noexcept
{
    return a.native_handle() == b.native_handle();
}
template <typename T, typename U> bool operator!=(const basic_easy<T>& a, const basic_easy<U>& b) noexcept
{
    return !operator==(a, b);
}
template <typename T> void swap(basic_easy<T>& a, basic_easy<T>& b) noexcept
{
    a.swap(b);
}

template <typename T, typename H> void operator>>(basic_easy<H>& handle, T&& obj)
{
    handle.response(obj);
    try {
        handle.perform();
        handle.response();
    } catch (...) {
        handle.response();
        throw;
    }
}
template <typename T, typename H> void operator>>(basic_easy<H>&& handle, T&& obj)
{
    auto h = std::move(handle);
    h.response(obj);
    h.perform();
}

#if LIBCURL_VERSION_NUM >= 0x073800
template<typename T> mime::mime(basic_easy<T>& easy) noexcept : mime(easy.native_handle()) {}
#endif

//-----------------------------------------------------------------------------
// time utilities

template <typename T> constexpr T to_msec(const timeval& tv)
{
    return static_cast<T>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}
template <typename T, typename R, typename P> constexpr T to_msec(const std::chrono::duration<R, P>& du)
{
    return static_cast<T>(std::chrono::duration_cast<std::chrono::milliseconds>(du).count());
}
constexpr timeval msec_to_timeval(time_t msec)
{
    return timeval {
        static_cast<decltype(timeval::tv_sec)>(msec / 1000), 
        static_cast<decltype(timeval::tv_usec)>((msec % 1000) * 1000)
    };
}
template <typename R, typename P> constexpr timeval to_timeval(const std::chrono::duration<R, P>& du)
{
    using namespace std::chrono;
    return timeval {
        static_cast<decltype(timeval::tv_sec)> (duration_cast<seconds>(du).count()), 
        static_cast<decltype(timeval::tv_usec)>(duration_cast<microseconds>(du % seconds(1)).count()) 
    };
}
template <typename ToDuration> constexpr ToDuration duration_cast(const timeval& tv)
{
    using namespace std::chrono;
    return duration_cast<ToDuration>(seconds(tv.tv_sec))
        +  duration_cast<ToDuration>(microseconds(tv.tv_usec));
}

//-----------------------------------------------------------------------------
// fdset & select

struct fdsets
{
#if defined(_MSC_VER)
    static const int MAX_FD = WSA_MAXIMUM_WAIT_EVENTS;
#else
    static const int MAX_FD = FD_SETSIZE;
#endif
    fdsets()
    {
        zero();
    }
    explicit operator bool() const noexcept
    {
        return maxfd < 0;
    }
    void zero() noexcept
    {
        maxfd = -1;
        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
    }
    int select(timeval& timeout) noexcept
    {
        return ::select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    }
    int select(time_t timeout_ms) noexcept
    {
        auto tv = msec_to_timeval(timeout_ms);
        return select(tv);
    }
    template <typename R, typename P> int select(const std::chrono::duration<R,P>& timeout) noexcept
    {
        auto tv = to_timeval(timeout);
        return select(tv);
    }

    int maxfd;
    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
};

//-----------------------------------------------------------------------------
// multi interface

template <typename Handle> class basic_multi;

// "uc::curl::multi" calls curl_multi_cleanup in the destructor. 
using multi = basic_multi<safe_ptr<detail::multi_handle>>;

// "uc::curl::multi_ref" does not release resources. It is for reference only.
using multi_ref = basic_multi<detail::multi_handle*>;


namespace detail
{
    //-----------------------------------------------------------------------------
    // CURLMoption policy , setopt()

    template <int OptionType> constexpr bool is_type(CURLMoption opt) noexcept {return (OptionType <= opt) && (opt < OptionType + 10000); }
    constexpr bool is_long(CURLMoption opt) noexcept {return is_type<CURLOPTTYPE_LONG>(opt); }
    constexpr bool is_objptr(CURLMoption opt) noexcept {return is_type<CURLOPTTYPE_OBJECTPOINT>(opt); }
    constexpr bool is_funcptr(CURLMoption opt) noexcept {return is_type<CURLOPTTYPE_FUNCTIONPOINT>(opt); }
    constexpr bool is_off_t(CURLMoption opt)  noexcept {return is_type<CURLOPTTYPE_OFF_T>(opt); }

    // type safe setopt()
    template <CURLMoption Option, sfinae<is_long(Option)> = nullptr>
    CURLMcode setopt(CURLM* handle, long parameter) noexcept
    {
        return curl_multi_setopt(handle, Option, parameter);
    }
    template <CURLMoption Option, typename T, sfinae<is_objptr(Option) && std::is_pointer<decay_t<T>>::value> = nullptr>
    CURLMcode setopt(CURLM* handle, T&& parameter) noexcept
    {
        return curl_multi_setopt(handle, Option, parameter);
    }
    template <CURLMoption Option, typename T, sfinae<is_objptr(Option) && is_same_decay<T, std::nullptr_t>::value> = nullptr>
    CURLMcode setopt(CURLM* handle, T&&) noexcept
    {
        return curl_multi_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLMoption Option, typename T, sfinae<is_objptr(Option) && is_same_decay<T, std::string>::value> = nullptr>
    CURLMcode setopt(CURLM* handle, T&& str) noexcept
    {
        return curl_multi_setopt(handle, Option, str.c_str());
    }
    template <CURLMoption Option, typename T, sfinae<is_funcptr(Option) && std::is_function<typename std::remove_pointer<T>::type>::value> = nullptr>
    CURLMcode setopt(CURLM* handle, T parameter) noexcept
    {
        return curl_multi_setopt(handle, Option, parameter);
    }
    template <CURLMoption Option, typename T, sfinae<is_funcptr(Option) && is_same_decay<T, std::nullptr_t>::value> = nullptr>
    CURLMcode setopt(CURLM* handle, T&&) noexcept
    {
        return curl_multi_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLMoption Option, sfinae<is_off_t(Option)> = nullptr>
    CURLMcode setopt(CURLM* handle, curl_off_t parameter) noexcept
    {
        return curl_multi_setopt(handle, Option, parameter);
    }

    // type safe clear function
    template <CURLMoption Option, sfinae<is_long(Option)> = nullptr>
    CURLMcode clearopt(CURL* handle) noexcept
    {
        return curl_multi_setopt(handle, Option, 0L);
    }
    template <CURLMoption Option, sfinae<is_objptr(Option)> = nullptr>
    CURLMcode clearopt(CURL* handle) noexcept
    {
        return curl_multi_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLMoption Option, sfinae<is_funcptr(Option)> = nullptr>
    CURLMcode clearopt(CURL* handle) noexcept
    {
        return curl_multi_setopt(handle, Option, static_cast<void*>(0));
    }
    template <CURLMoption Option, sfinae<is_off_t(Option)> = nullptr>
    CURLMcode clearopt(CURL* handle) noexcept
    {
        return curl_multi_setopt(handle, Option, static_cast<curl_off_t>(0));
    }
}

template <typename Handle> class basic_multi
{
public:
    using handle_type = Handle;
    using pointer = CURLM*;

    // multi only
    basic_multi();

    // multi_ref only
    basic_multi(CURLM*);

    // common
    ~basic_multi() noexcept = default;
    basic_multi(const basic_multi&) = delete;
    basic_multi(basic_multi&& obj) noexcept = default;
    basic_multi& operator=(const basic_multi&) = delete;
    basic_multi& operator=(basic_multi&& obj) noexcept = default;

    explicit operator bool() const noexcept { return static_cast<bool>(handle); }
    pointer native_handle() const noexcept { return handle.get(); }
    void swap(basic_multi& obj) { handle.swap(obj.handle); }

    //! @param func void (uc::curl::multi_ref multi, long timeout_ms)
    template <typename F> basic_multi& on_timer(F& func)
    {
        return setopt(CURLMOPT_TIMERDATA, &func).setopt(CURLMOPT_TIMERFUNCTION, &basic_multi::timer_cb<F>);
    }
    //! @param func void (uc::curl::easy_ref easy, curl_socket_t sockfd, int action, T* sockptr)
    template <typename T, typename F> basic_multi& on_socket(F& func)
    {
        return setopt(CURLMOPT_SOCKETDATA, &func).setopt(CURLMOPT_SOCKETFUNCTION, &basic_multi::socket_cb<T, F>);
    }
    //! @param func int (uc::curl::easy_ref parent, uc::curl::easy_ref easy, size_t num_headers, curl_pushheaders *headers)
    template <typename T, typename F> basic_multi& on_push(F& func)
    {
        return setopt(CURLMOPT_PUSHDATA, &func).setopt(CURLMOPT_PUSHFUNCTION, &basic_multi::push_cb<F>);
    }


    template <CURLMoption Option> basic_multi& setopt()
    {
        UC_CURL_ASSERT_CURLMCODE(detail::setopt<Option>(native_handle(), 1L));
        return *this;
    }
    template <CURLMoption Option> basic_multi& clear()
    {
        UC_CURL_ASSERT_CURLMCODE(detail::clearopt<Option>(native_handle()));
        return *this;
    }
    template <CURLMoption Option, typename T> basic_multi& setopt(T&& parameter)
    {
        UC_CURL_ASSERT_CURLMCODE(detail::setopt<Option>(native_handle(), std::forward<T>(parameter)));
        return *this;
    }
    template <typename T> basic_multi& setopt(CURLMoption option, const T& parameter)
    {
       UC_CURL_ASSERT_CURLMCODE(curl_multi_setopt(native_handle(), option, parameter));
        return *this;
    }

    template <typename T> basic_multi& add(basic_easy<T>& e)
    {
        UC_CURL_ASSERT_CURLMCODE(curl_multi_add_handle(native_handle(), e.native_handle()));
        return *this;
    }
    template <typename T> basic_multi& remove(basic_easy<T>& e)
    {
        UC_CURL_ASSERT_CURLMCODE(curl_multi_remove_handle(native_handle(), e.native_handle()));
        return *this;
    }

    basic_multi& assign(curl_socket_t sockfd, void* sockptr)
    {
        UC_CURL_ASSERT_CURLMCODE(curl_multi_assign(native_handle(), sockfd, sockptr));
        return *this;
    }
    // curl_multi_socket() and curl_multi_socket_all() are deprecated.
    int socket_action(curl_socket_t sockfd, int ev_bitmask)
    {
        int running_handles{};
        UC_CURL_ASSERT_CURLMCODE(curl_multi_socket_action(native_handle(), sockfd, ev_bitmask, &running_handles));
        return running_handles;
    }
    int socket_action_timeout()
    {
        return socket_action(CURL_SOCKET_TIMEOUT, 0);
    }

    int perform()
    {
        int running_handles;
        UC_CURL_ASSERT_CURLMCODE(curl_multi_perform(native_handle(), &running_handles));
        return running_handles;
    }
    void fdset(fdsets& sets)
    {
        int maxfd = -1;
        UC_CURL_ASSERT_CURLMCODE(curl_multi_fdset(native_handle(), &sets.fdread, &sets.fdwrite, &sets.fdexcep, &maxfd));
        sets.maxfd = std::max(sets.maxfd, maxfd);
    }

    long timeout_ms()
    {
        long ret = 0;
        UC_CURL_ASSERT_CURLMCODE(curl_multi_timeout(native_handle(), &ret));
        return ret;
    }
    std::chrono::milliseconds timeout()
    {
        return std::chrono::milliseconds(timeout_ms());
    }

    void wakeup()
    {
        UC_CURL_ASSERT_CURLMCODE(curl_multi_wakeup(native_handle()));
    }

    // timeout : milliseconds(int) or std::chrono::duration
    template <typename T> int poll(const T& timeout)
    {
        return poll(nullptr, 0, timeout);
    }
    template <typename Container, typename T> int poll(Container& extra_fds, const T& timeout)
    {
        return poll(detail::data(extra_fds), detail::size<unsigned int>(extra_fds), timeout);
    }
    template <typename R, typename P> int poll(curl_waitfd extra_fds[], unsigned int extra_nfds, const std::chrono::duration<R, P>& timeout)
    {
        return poll(extra_fds, extra_nfds, to_msec<int>(timeout));
    }
    int poll(curl_waitfd extra_fds[], unsigned int extra_nfds, int timeout_ms)
    {
        int numfds{};
        UC_CURL_ASSERT_CURLMCODE(curl_multi_poll(native_handle(), extra_fds, extra_nfds, timeout_ms, &numfds));
        return numfds;
    }

    template <typename T> int wait(const T& timeout)
    {
        return wait(nullptr, 0, timeout);
    }
    template <typename Container, typename T> int wait(Container& extra_fds, const T& timeout)
    {
        return wait(detail::data(extra_fds), detail::size<unsigned int>(extra_fds), timeout);
    }
    template <typename R, typename P> int wait(curl_waitfd extra_fds[], unsigned int extra_nfds, const std::chrono::duration<R, P>& timeout)
    {
        return wait(extra_fds, extra_nfds, to_msec<int>(timeout));
    }
    int wait(curl_waitfd extra_fds[], unsigned int extra_nfds, int timeout_ms)
    {
        int numfds{};
        UC_CURL_ASSERT_CURLMCODE(curl_multi_wait(native_handle(), extra_fds, extra_nfds, timeout_ms, &numfds));
        return numfds;
    }

    //! @param func void (easy_ref, CURLMSG, CURLcode)
    template <typename F> void for_each_info(F func)
    {
        int msgs_in_queue = 0;
        for (auto msg = info_read(&msgs_in_queue); msg; msg = info_read(&msgs_in_queue)) {
            func(easy_ref{msg->easy_handle}, msg->msg, msg->data.result);
        }
    }
    //! @param func void (easy_ref, CURLcode)
    template <typename F> void for_each_done_info(F func)
    {
        int msgs_in_queue = 0;
        for (auto msg = info_read(&msgs_in_queue); msg; msg = info_read(&msgs_in_queue)) {
            if (msg->msg == CURLMSG_DONE) {
                func(easy_ref{msg->easy_handle}, msg->data.result);
            }
        }
    }
    const CURLMsg* info_read() noexcept
    {
        int dummy = 0;
        return curl_multi_info_read(native_handle(), &dummy);
    }
    const CURLMsg* info_read(int* msgs_in_queue) noexcept
    {
        return curl_multi_info_read(native_handle(), msgs_in_queue);
    }
private:
    template <typename F> static int timer_cb(CURLM *multi, long timeout_ms, void *userp)
    {
        try {
            (*static_cast<F*>(userp))(multi_ref{multi}, timeout_ms);
        } catch (...) {}
        return 0;
    }
    template <typename T, typename F> static int socket_cb(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) noexcept
    {
        try {
            (*static_cast<F*>(userp))(easy_ref{easy}, s, action, static_cast<T*>(socketp));
        } catch (...) {}
        return 0;
    }
    template <typename F> static int push_cb(CURL *parent, CURL *easy, size_t num_headers, curl_pushheaders *headers, void *userp) noexcept
    {
        try {
            return (*static_cast<F*>(userp))(easy_ref{parent}, easy_ref{easy}, num_headers, headers);
        } catch (...) {}
        return CURL_PUSH_DENY;
    }
    handle_type handle;
};
template<> inline multi::basic_multi() : handle{static_cast<detail::multi_handle*>(curl_multi_init())}
{
    UC_CURL_ASSERT(handle, CURLE_FAILED_INIT);
}
template<> inline multi_ref::basic_multi(CURLM* h) : handle{static_cast<detail::multi_handle*>(h)}
{
    UC_CURL_ASSERT(handle, CURLE_BAD_FUNCTION_ARGUMENT);
}
template <typename T> void swap(basic_multi<T>& a, basic_multi<T>& b) noexcept
{
    a.swap(b);
}
}
}
#endif
