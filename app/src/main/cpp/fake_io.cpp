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

#define LOG_TAG "FakeIO_Final"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 1. 配置路径信息
const char* REDIRECT_TARGET = "/data/user/0/com.bilibili.azurlane/files/base_orig.apk";
const char* APK_PATH_PREFIX = "/data/app/";
const char* BASE_APK_NAME = "base.apk";
const char* EXPECTED_LEGIT_PATH = "/data/app/com.bilibili.azurlane/base.apk";

// 2. 原函数指针
int (*orig_open)(const char*, int, mode_t) = nullptr;
int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;
ssize_t (*orig_readlink)(const char*, char*, size_t) = nullptr;
ssize_t (*orig_readlinkat)(int, const char*, char*, size_t) = nullptr;
char* (*orig_fgets)(char*, int, FILE*) = nullptr;
ssize_t (*orig_read)(int, void*, size_t) = nullptr;
int (*orig_close)(int) = nullptr;

// 3. 状态追踪：记录 maps 文件的句柄
static int g_maps_fd = -1;

/**
 * 核心判定：是否需要重定向到原版 APK 副本
 */
bool should_redirect(const char* pathname) {
    if (pathname == nullptr) return false;
    const char* pos = strstr(pathname, BASE_APK_NAME);
    if (strstr(pathname, APK_PATH_PREFIX) != nullptr && pos != nullptr) {
        if (pos[8] == '\0') return true;
    }
    return false;
}

/**
 * 核心判定：路径是否泄露了副本位置
 */
bool is_leaked_path(const char* buf) {
    return buf && strstr(buf, "base_orig.apk") != nullptr;
}

// === Hook 实现 ===

int my_open(const char* pathname, int flags, mode_t mode) {
    if (should_redirect(pathname)) {
        LOGI(">>> Open Redirect: %s -> %s", pathname, REDIRECT_TARGET);
        return orig_open(REDIRECT_TARGET, flags, mode);
    }
    int fd = orig_open(pathname, flags, mode);
    // 追踪 maps 文件的打开
    if (pathname && strstr(pathname, "/proc/self/maps")) {
        g_maps_fd = fd;
    }
    return fd;
}

int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (should_redirect(pathname)) {
        LOGI(">>> Openat Redirect: %s", pathname);
        return orig_openat(dirfd, REDIRECT_TARGET, flags, mode);
    }
    int fd = orig_openat(dirfd, pathname, flags, mode);
    if (pathname && strstr(pathname, "/proc/self/maps")) {
        g_maps_fd = fd;
    }
    return fd;
}

int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    if (should_redirect(pathname)) {
        int res = orig_newfstatat(dirfd, REDIRECT_TARGET, buf, flags);
        if (res == 0) LOGI(">>> Stat Fake: %s Size: %lld", pathname, (long long)buf->st_size);
        return res;
    }
    return orig_newfstatat(dirfd, pathname, buf, flags);
}

// 修正 readlink (/proc/self/fd 幻术)
ssize_t my_readlink(const char* pathname, char* buf, size_t bufsiz) {
    ssize_t res = orig_readlink(pathname, buf, bufsiz);
    if (res > 0 && is_leaked_path(buf)) {
        LOGI(">>> Readlink Illusion Triggered");
        strncpy(buf, EXPECTED_LEGIT_PATH, bufsiz);
        return strlen(buf);
    }
    return res;
}

ssize_t my_readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz) {
    ssize_t res = orig_readlinkat(dirfd, pathname, buf, bufsiz);
    if (res > 0 && is_leaked_path(buf)) {
        LOGI(">>> Readlinkat Illusion Triggered");
        strncpy(buf, EXPECTED_LEGIT_PATH, bufsiz);
        return strlen(buf);
    }
    return res;
}

// 修正 fgets (基于行的 maps 扫描屏蔽)
char* my_fgets(char* s, int size, FILE* stream) {
    char* res = orig_fgets(s, size, stream);
    if (res && is_leaked_path(s)) {
        char* p = strstr(s, REDIRECT_TARGET);
        if (p) {
            LOGI(">>> Fgets Maps Illusion: Path Corrected");
            memset(p, ' ', strlen(REDIRECT_TARGET)); 
            memcpy(p, EXPECTED_LEGIT_PATH, strlen(EXPECTED_LEGIT_PATH));
        }
    }
    return res;
}

// 修正 read (底层字节流 maps 扫描屏蔽)
ssize_t my_read(int fd, void* buf, size_t count) {
    ssize_t res = orig_read(fd, buf, count);
    if (res > 0 && fd == g_maps_fd && g_maps_fd != -1) {
        char* content = (char*)buf;
        char* p = strstr(content, "base_orig.apk");
        if (p) {
            LOGI(">>> Read Maps Illusion: Patching Byte Stream");
            // 查找这一段路径的起始，并原地替换
            char* start = strstr(content, "/data/user/0/"); 
            if (start) {
                // 用空格填充旧路径并写入伪装路径
                memset(start, ' ', 60); 
                memcpy(start, EXPECTED_LEGIT_PATH, strlen(EXPECTED_LEGIT_PATH));
            }
        }
    }
    return res;
}

int my_close(int fd) {
    if (fd == g_maps_fd) g_maps_fd = -1;
    return orig_close(fd);
}

__attribute__((constructor))
void init() {
    // 使用 Dobby 进行动态 Hook
    #define HOOK_FUNC(name, my_func, orig_func) \
        void* addr_##name = dlsym(RTLD_DEFAULT, #name); \
        if (addr_##name) DobbyHook(addr_##name, (dobby_dummy_func_t)my_func, (dobby_dummy_func_t*)&orig_func);

    HOOK_FUNC(open, my_open, orig_open);
    HOOK_FUNC(openat, my_openat, orig_openat);
    HOOK_FUNC(readlink, my_readlink, orig_readlink);
    HOOK_FUNC(readlinkat, my_readlinkat, orig_readlinkat);
    HOOK_FUNC(fgets, my_fgets, orig_fgets);
    HOOK_FUNC(read, my_read, orig_read);
    HOOK_FUNC(close, my_close, orig_close);

    // Stat 系列兼容处理
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat");
    if (stat_ptr) DobbyHook(stat_ptr, (dobby_dummy_func_t)my_newfstatat, (dobby_dummy_func_t*)&orig_newfstatat);

    LOGI("FakeIO: [Ultra Mode] Fully Initialized.");
}
