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
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char* REDIRECT_TARGET = "/data/user/0/com.bilibili.azurlane/files/base_orig.apk";
const char* APK_PATH_PREFIX = "/data/app/";
const char* BASE_APK_NAME = "base.apk";

int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;

/**
 * 精准重定向判断
 */
bool should_redirect(const char* pathname) {
    if (pathname == nullptr) return false;
    
    // 1. 必须包含 /data/app/
    if (strstr(pathname, APK_PATH_PREFIX) == nullptr) return false;

    // 2. 找到 base.apk 的位置
    const char* pos = strstr(pathname, BASE_APK_NAME);
    if (pos == nullptr) return false;

    // 3. 核心修复逻辑：
    // 如果是访问整体 APK，路径通常以 "base.apk" 结尾，pos[8] 应该是 '\0'。
    // 如果是在访问内部资源，路径通常是 ".../base.apk/assets/..."，此时 pos[8] 是 '/'。
    // 如果是 Zip 访问，路径通常是 ".../base.apk!/..."，此时 pos[8] 是 '!'。
    
    char next_char = pos[8]; // "base.apk" 长度为 8
    
    if (next_char == '\0') {
        // 只有当路径正好以 base.apk 结尾时才重定向（用于签名和大小校验）
        return true;
    }

    // 对于 base.apk/ 或 base.apk! 开头的路径，直接放行，让其访问修改版 APK 内部的真实资源
    return false;
}

int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (should_redirect(pathname)) {
        int res = orig_openat(dirfd, REDIRECT_TARGET, flags, mode);
        if (res != -1) {
            LOGI("拦截成功 (openat): %s -> %s", pathname, REDIRECT_TARGET);
        } else {
            LOGE("拦截失败 (openat): 无法打开目标文件 %s, 错误: %s", REDIRECT_TARGET, strerror(errno));
        }
        return res;
    }
    return orig_openat(dirfd, pathname, flags, mode);
}

int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    if (should_redirect(pathname)) {
        int res = orig_newfstatat(dirfd, REDIRECT_TARGET, buf, flags);
        if (res != -1) {
            LOGI("拦截成功 (stat): %s -> %s, 大小: %lld", pathname, REDIRECT_TARGET, (long long)buf->st_size);
        } else {
            LOGE("拦截失败 (stat): 无法获取目标属性 %s, 错误: %s", REDIRECT_TARGET, strerror(errno));
        }
        return res;
    }
    return orig_newfstatat(dirfd, pathname, buf, flags);
}

__attribute__((constructor))
void init() {
    void* openat_ptr = dlsym(RTLD_DEFAULT, "openat");
    
    // 尝试适配不同系统的 stat 符号
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat");

    if (openat_ptr) {
        DobbyHook(openat_ptr, (dobby_dummy_func_t)my_openat, (dobby_dummy_func_t*)&orig_openat);
    }
    if (stat_ptr) {
        DobbyHook(stat_ptr, (dobby_dummy_func_t)my_newfstatat, (dobby_dummy_func_t*)&orig_newfstatat);
    }
    
    LOGI("FakeIO: 精准拦截模式已部署 (修复 Unity 资源加载冲突)");
}
