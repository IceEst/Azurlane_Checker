#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

extern "C" {
    #include "dobby.h"
}

#define LOG_TAG "FakeIO_Check"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 伪装目标路径（副本）
const char* REDIRECT_TARGET = "/data/user/0/com.bilibili.azurlane/files/base_orig.apk";
// 原始路径关键字（用于匹配）
const char* APK_PATH_PREFIX = "/data/app/";
const char* BASE_APK_NAME = "base.apk";

// 期望显示的路径（幻术：让系统看起来依然在读这个路径）
// 注意：这个路径最好通过动态获取，这里先根据你的日志硬编码一个常见的格式
const char* EXPECTED_LEGIT_PATH = "/data/app/com.bilibili.azurlane/base.apk";

// 原函数指针
int (*orig_open)(const char*, int, mode_t) = nullptr;
int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;
ssize_t (*orig_readlink)(const char*, char*, size_t) = nullptr;
ssize_t (*orig_readlinkat)(int, const char*, char*, size_t) = nullptr;
char* (*orig_fgets)(char*, int, FILE*) = nullptr;

/**
 * 判断是否是针对 APK 文件本身的访问（用于 open/stat）
 */
bool should_redirect(const char* pathname) {
    if (pathname == nullptr) return false;
    const char* pos = strstr(pathname, BASE_APK_NAME);
    if (strstr(pathname, APK_PATH_PREFIX) != nullptr && pos != nullptr) {
        if (pos[8] == '\0') {
            return true;
        }
    }
    return false;
}

/**
 * 判断路径是否包含我们的伪装目标（用于 readlink/maps 修正）
 */
bool is_redirected_path(const char* path) {
    return (path && strstr(path, "base_orig.apk") != nullptr);
}

// 拦截 open
int my_open(const char* pathname, int flags, mode_t mode) {
    if (should_redirect(pathname)) {
        LOGI(">>> 拦截到 open: %s -> 重定向", pathname);
        return orig_open(REDIRECT_TARGET, flags, mode);
    }
    return orig_open(pathname, flags, mode);
}

// 拦截 openat
int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (should_redirect(pathname)) {
        LOGI(">>> 拦截到 openat: %s -> 重定向", pathname);
        return orig_openat(dirfd, REDIRECT_TARGET, flags, mode);
    }
    return orig_openat(dirfd, pathname, flags, mode);
}

// 拦截 stat
int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    if (should_redirect(pathname)) {
        int res = orig_newfstatat(dirfd, REDIRECT_TARGET, buf, flags);
        if (res == 0) {
            LOGI(">>> 拦截到 stat: %s, 伪装大小: %lld", pathname, (long long)buf->st_size);
        }
        return res;
    }
    return orig_newfstatat(dirfd, pathname, buf, flags);
}

// 拦截 readlink (解决 /proc/self/fd/ 溯源)
ssize_t my_readlink(const char* pathname, char* buf, size_t bufsiz) {
    ssize_t res = orig_readlink(pathname, buf, bufsiz);
    if (res > 0 && is_redirected_path(buf)) {
        LOGI(">>> 拦截到 readlink 泄露，正在修正路径");
        strncpy(buf, EXPECTED_LEGIT_PATH, bufsiz);
        return strlen(buf);
    }
    return res;
}

// 拦截 readlinkat
ssize_t my_readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz) {
    ssize_t res = orig_readlinkat(dirfd, pathname, buf, bufsiz);
    if (res > 0 && is_redirected_path(buf)) {
        LOGI(">>> 拦截到 readlinkat 泄露，正在修正路径");
        strncpy(buf, EXPECTED_LEGIT_PATH, bufsiz);
        return strlen(buf);
    }
    return res;
}

// 拦截 fgets (解决 /proc/self/maps 泄露)
char* my_fgets(char* s, int size, FILE* stream) {
    char* res = orig_fgets(s, size, stream);
    if (res != nullptr) {
        // 性能优化：仅在行内包含关键词时处理
        if (strstr(s, "base_orig.apk") != nullptr) {
            // 查找起始位置
            char* p = strstr(s, REDIRECT_TARGET);
            if (p) {
                LOGI(">>> 发现 Maps 泄露行，正在原地修正");
                // 简单的原地替换（幻术）：将 base_orig.apk 路径替换回官方 base.apk 路径
                // 注意：如果 EXPECTED_LEGIT_PATH 较短，后面可以用空格填充以保持对齐
                memset(p, 0, strlen(REDIRECT_TARGET)); 
                memcpy(p, EXPECTED_LEGIT_PATH, strlen(EXPECTED_LEGIT_PATH));
            }
        }
    }
    return res;
}

__attribute__((constructor))
void init() {
    // 1. Hook open / openat
    void* open_ptr = dlsym(RTLD_DEFAULT, "open");
    if (open_ptr) DobbyHook(open_ptr, (dobby_dummy_func_t)my_open, (dobby_dummy_func_t*)&orig_open);

    void* openat_ptr = dlsym(RTLD_DEFAULT, "openat");
    if (openat_ptr) DobbyHook(openat_ptr, (dobby_dummy_func_t)my_openat, (dobby_dummy_func_t*)&orig_openat);

    // 2. Hook stat 系列
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat");
    if (stat_ptr) DobbyHook(stat_ptr, (dobby_dummy_func_t)my_newfstatat, (dobby_dummy_func_t*)&orig_newfstatat);

    // 3. Hook readlink 系列 (解决 FD 指向问题)
    void* rl_ptr = dlsym(RTLD_DEFAULT, "readlink");
    if (rl_ptr) DobbyHook(rl_ptr, (dobby_dummy_func_t)my_readlink, (dobby_dummy_func_t*)&orig_readlink);

    void* rlat_ptr = dlsym(RTLD_DEFAULT, "readlinkat");
    if (rlat_ptr) DobbyHook(rlat_ptr, (dobby_dummy_func_t)my_readlinkat, (dobby_dummy_func_t*)&orig_readlinkat);

    // 4. Hook fgets (解决 Maps 路径问题)
    void* fgets_ptr = dlsym(RTLD_DEFAULT, "fgets");
    if (fgets_ptr) DobbyHook(fgets_ptr, (dobby_dummy_func_t)my_fgets, (dobby_dummy_func_t*)&orig_fgets);

    LOGI("FakeIO: 全方位拦截模式已就绪 (包含 open/stat/readlink/maps)");
}
