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

#define LOG_TAG "FakeIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

const char* REDIRECT_TARGET = "/data/user/0/com.bilibili.azurlane/files/base_orig.apk";
const char* APK_PATH_PREFIX = "/data/app/";
const char* BASE_APK_NAME = "base.apk";

int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;

bool should_redirect(const char* pathname) {
    if (pathname == nullptr) return false;
    const char* pos = strstr(pathname, BASE_APK_NAME);
    if (strstr(pathname, APK_PATH_PREFIX) != nullptr && pos != nullptr) {
        // 只在路径以 base.apk 结尾时重定向，排除 base.apk/ 或 base.apk!
        return (pos[8] == '\0');
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
    LOGI("FakeIO: 仅拦截 APK 文件本身访问模式已启用");
}
