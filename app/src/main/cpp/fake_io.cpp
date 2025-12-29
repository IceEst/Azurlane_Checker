#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <android/log.h>
#include <dlfcn.h> // 必须引入，用于 dlsym
#include "dobby.h"

#define LOG_TAG "FakeIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

const char* REDIRECT_TARGET = "/data/data/com.bilibili.azurlane/base_orig.apk";

int (*orig_openat)(int, const char*, int, mode_t);
int (*orig_newfstatat)(int, const char*, struct stat*, int);

const char* redirect_if_needed(const char* pathname) {
    if (pathname != nullptr && strstr(pathname, "base.apk") != nullptr) {
        // 排除掉 .so 库的探测路径，只拦截包体访问
        if (strstr(pathname, "!") == nullptr) {
            LOGI("正在重定向包体访问: %s", pathname);
            return REDIRECT_TARGET;
        }
    }
    return pathname;
}

int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    return orig_openat(dirfd, redirect_if_needed(pathname), flags, mode);
}

int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    return orig_newfstatat(dirfd, redirect_if_needed(pathname), buf, flags);
}

__attribute__((constructor))
void init() {
    LOGI("FakeIO: 正在通过 dlsym 寻找函数地址...");

    // 使用更稳定的 RTLD_DEFAULT 寻找 libc 函数
    void* openat_ptr = dlsym(RTLD_DEFAULT, "openat");
    // 在 64 位系统上，stat 往往映射为 fstatat 或 newfstatat
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (stat_ptr == nullptr) {
        stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");
    }

    // 只有找到地址后才执行 Hook，防止空指针闪退
    if (openat_ptr != nullptr) {
        DobbyHook(openat_ptr, (void*)my_openat, (void**)&orig_openat);
        LOGI("FakeIO: openat Hook 部署成功");
    }

    if (stat_ptr != nullptr) {
        DobbyHook(stat_ptr, (void*)my_newfstatat, (void**)&orig_newfstatat);
        LOGI("FakeIO: newfstatat Hook 部署成功");
    }
}
