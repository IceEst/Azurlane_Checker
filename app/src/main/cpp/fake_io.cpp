#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>

extern "C" {
    #include "dobby.h"
}

#define LOG_TAG "FakeIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 建议将备份文件放在 files 目录下，权限更稳定
const char* REDIRECT_TARGET = "/data/user/0/com.bilibili.azurlane/files/base_orig.apk";
const char* APK_PATH_PREFIX = "/data/app/";
const char* BASE_APK_NAME = "base.apk";

int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;

// 精准重定向判断
bool should_redirect(const char* pathname) {
    if (pathname == nullptr) return false;
    
    // 1. 路径必须包含 /data/app/ 且包含 base.apk (确保不是 dalvik-cache)
    // 2. 路径绝对不能包含 !/ (确保是读取整个 APK 文件，而不是加载内部资源或 .so)
    if (strstr(pathname, APK_PATH_PREFIX) != nullptr && 
        strstr(pathname, BASE_APK_NAME) != nullptr &&
        strstr(pathname, "!") == nullptr) {
        return true;
    }
    return false;
}

int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (should_redirect(pathname)) {
        LOGI("拦截 APK 访问: %s -> %s", pathname, REDIRECT_TARGET);
        return orig_openat(dirfd, REDIRECT_TARGET, flags, mode);
    }
    return orig_openat(dirfd, pathname, flags, mode);
}

int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    if (should_redirect(pathname)) {
        LOGI("拦截 APK 属性查询: %s -> %s", pathname, REDIRECT_TARGET);
        return orig_newfstatat(dirfd, REDIRECT_TARGET, buf, flags);
    }
    return orig_newfstatat(dirfd, pathname, buf, flags);
}

__attribute__((constructor))
void init() {
    void* openat_ptr = dlsym(RTLD_DEFAULT, "openat");
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");

    if (openat_ptr) {
        DobbyHook(openat_ptr, (dobby_dummy_func_t)my_openat, (dobby_dummy_func_t*)&orig_openat);
    }
    if (stat_ptr) {
        DobbyHook(stat_ptr, (dobby_dummy_func_t)my_newfstatat, (dobby_dummy_func_t*)&orig_newfstatat);
    }
    LOGI("FakeIO: 精准 Hook 部署完成");
}
