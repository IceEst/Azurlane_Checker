#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <android/log.h>
#include "dobby.h"

#define LOG_TAG "FakeIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 原版备份路径 (请确保手动将原版 base.apk 改名放进这个位置)
const char* REDIRECT_TARGET = "/data/data/com.bilibili.azurlane/base_orig.apk";

// 备份原始函数地址
int (*orig_openat)(int, const char*, int, mode_t);
int (*orig_newfstatat)(int, const char*, struct stat*, int);

// 核心重定向逻辑
const char* redirect_if_needed(const char* pathname) {
    if (pathname != nullptr && strstr(pathname, "base.apk") != nullptr) {
        LOGI("发现目标路径，执行重定向: %s", pathname);
        return REDIRECT_TARGET;
    }
    return pathname;
}

// 伪造函数：openat
int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    return orig_openat(dirfd, redirect_if_needed(pathname), flags, mode);
}

// 伪造函数：newfstatat
int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    return orig_newfstatat(dirfd, redirect_if_needed(pathname), buf, flags);
}

// 模块加载时的初始化
__attribute__((constructor))
void init() {
    LOGI("Native Hook 模块已载入，正在搜寻目标地址...");
    
    // 使用 Dobby 挂钩系统调用
    DobbyHook((void*)DobbySymbolResolver("libc.so", "openat"), 
              (void*)my_openat, (void**)&orig_openat);
              
    DobbyHook((void*)DobbySymbolResolver("libc.so", "newfstatat"), 
              (void*)my_newfstatat, (void**)&orig_newfstatat);
              
    LOGI("Hook 部署完成");
}
