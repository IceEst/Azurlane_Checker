#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>

extern "C" {
    #include "dobby.h"
}

#define LOG_TAG "FakeIO_Check"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char* REDIRECT_TARGET = "/data/user/0/com.bilibili.azurlane/files/base_orig.apk";
const char* APK_PATH_PREFIX = "/data/app/";
const char* BASE_APK_NAME = "base.apk";

// 原函数指针
int (*orig_open)(const char*, int, mode_t) = nullptr;
int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;

/**
 * 核心逻辑：判断是否是针对 APK 文件本身的访问
 */
bool should_redirect(const char* pathname) {
    if (pathname == nullptr) return false;
    
    // 包含 /data/app/ 且包含 base.apk
    const char* pos = strstr(pathname, BASE_APK_NAME);
    if (strstr(pathname, APK_PATH_PREFIX) != nullptr && pos != nullptr) {
        // 精准匹配：base.apk 后面必须是字符串结束符
        // 排除 base.apk/assets... 或 base.apk!/...
        if (pos[8] == '\0') {
            return true;
        }
    }
    return false;
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
    
    // 调试：如果包含 base.apk 但被放行了，也打个日志观察一下
    if (pathname && strstr(pathname, BASE_APK_NAME)) {
        // LOGI("放行资源访问: %s", pathname);
    }
    
    return orig_newfstatat(dirfd, pathname, buf, flags);
}

__attribute__((constructor))
void init() {
    // 1. Hook open
    void* open_ptr = dlsym(RTLD_DEFAULT, "open");
    if (open_ptr) DobbyHook(open_ptr, (dobby_dummy_func_t)my_open, (dobby_dummy_func_t*)&orig_open);

    // 2. Hook openat
    void* openat_ptr = dlsym(RTLD_DEFAULT, "openat");
    if (openat_ptr) DobbyHook(openat_ptr, (dobby_dummy_func_t)my_openat, (dobby_dummy_func_t*)&orig_openat);

    // 3. Hook stat 系列 (尝试多种符号)
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat");
    
    if (stat_ptr) {
        DobbyHook(stat_ptr, (dobby_dummy_func_t)my_newfstatat, (dobby_dummy_func_t*)&orig_newfstatat);
    }

    LOGI("FakeIO: 深度拦截模式已就绪 (包含 open/openat/stat)");
}
