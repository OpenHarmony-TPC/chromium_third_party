/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "proc_info_ohos.h"
#include "third_party/bounds_checking_function/include/securec.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

namespace crashpad {
namespace {
const char PID_STR_NAME[] = "Pid:";
const char PPID_STR_NAME[] = "PPid:";
const char NSPID_STR_NAME[] = "NSpid:";
static const char PROC_SELF_STATUS_PATH[] = "/proc/self/status";
static const char PROC_SELF_TASK_PATH[] = "/proc/self/task";
const int ARGS_COUNT_ONE = 1;
const int ARGS_COUNT_TWO = 2;
const int STATUS_LINE_SIZE = 1024;
const int STRING_BUF_LEN = 4096;
const int PATH_LEN = 512;
}

static bool GetProcStatusByPath(struct ProcInfo& procInfo, const std::string& path)
{
    char buf[STATUS_LINE_SIZE];
    FILE *fp = fopen(path.c_str(), "r");
    if (fp == nullptr) {
        return false;
    }

    int pid = 0;
    int ppid = 0;
    int nsPid = 0;
    while (!feof(fp)) {
        if (fgets(buf, STATUS_LINE_SIZE, fp) == nullptr) {
            fclose(fp);
            return false;
        }

        if (strncmp(buf, PID_STR_NAME, strlen(PID_STR_NAME)) == 0) {
            // Pid:   1892
            if (sscanf_s(buf, "%*[^0-9]%d", &pid) != ARGS_COUNT_ONE) {
                procInfo.pid = getpid();
            } else {
                procInfo.pid = pid;
            }
            procInfo.nsPid = pid;
            continue;
        }

        if (strncmp(buf, PPID_STR_NAME, strlen(PPID_STR_NAME)) == 0) {
            // PPid:   240
            if (sscanf_s(buf, "%*[^0-9]%d", &ppid) != ARGS_COUNT_ONE) {
                procInfo.ppid = getppid();
            } else {
                procInfo.ppid = ppid;
            }
            continue;
        }

        if (strncmp(buf, NSPID_STR_NAME, strlen(NSPID_STR_NAME)) == 0) {
            // NSpid:  1892    1
            if (sscanf_s(buf, "%*[^0-9]%d%*[^0-9]%d", &pid, &nsPid) != ARGS_COUNT_TWO) {
                procInfo.ns = false;
                procInfo.nsPid = pid;
            } else {
                procInfo.ns = true;
                procInfo.nsPid = nsPid;
            }
            procInfo.pid = pid;
            break;
        }
    }
    (void)fclose(fp);
    return true;
}

static int BufferAppendV(char *buf, int size, const char *fmt, va_list ap)
{
    if (buf == nullptr || size <= 0) {
        return -1;
    }

    int ret = vsnprintf_s(buf, (size_t)size, (size_t)(size - 1), fmt, ap);

    if (ret == -1) {
        buf[size - 1] = '\0';
        return size - 1;
    }
    
    return ret;
}

static bool StringAppendV(std::string& dst, const char* fmt, va_list ap)
{
    char buffer[STRING_BUF_LEN] = {0};
    va_list bakAp;
    va_copy(bakAp, ap);
    int ret = BufferAppendV(buffer, sizeof(buffer), fmt, bakAp);
    va_end(bakAp);
    if (ret > 0) {
        dst.append(buffer, ret);
    }
    return ret != -1;
}

static std::string StringPrintf(const char *fmt, ...)
{
    if (fmt == nullptr) {
        return "";
    }
    std::string dst;
    va_list ap;
    va_start(ap, fmt);
    StringAppendV(dst, fmt, ap);
    va_end(ap);
    return dst;
}

bool TidToNstid(const int pid, const int tid, int& nstid)
{
    std::string path = StringPrintf("/proc/%d/task/%d/status", pid, tid);
    if (path.empty()) {
        return false;
    }

    struct ProcInfo procInfo;
    if (!GetProcStatusByPath(procInfo, path)) {
        return false;
    }
    nstid = procInfo.nsPid;
    return true;
}

bool GetProcStatusByPid(int realPid, struct ProcInfo& procInfo)
{
    std::string path = StringPrintf("/proc/%d/status", realPid);
    return GetProcStatusByPath(procInfo, path);
}

bool GetProcStatus(struct ProcInfo& procInfo)
{
    return GetProcStatusByPath(procInfo, PROC_SELF_STATUS_PATH);
}

bool IsThreadInPid(int32_t pid, int32_t tid)
{
    std::string path;
    if (pid == getpid()) {
        path = StringPrintf("%s/%d", PROC_SELF_TASK_PATH, tid);
    } else {
        path = StringPrintf("/proc/%d/task/%d", pid, tid);
    }
    return access(path.c_str(), F_OK) == 0;
}

bool ReadDirFiles(const std::string& path, std::vector<std::string>& files)
{
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        return false;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        // current dir OR parent dir
        if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0)) {
            continue;
        }
        files.emplace_back(std::string(ent->d_name));
    }
    (void)closedir(dir);
    return (files.size() > 0);
}

bool ReadDirFilesByPid(const int& pid, std::vector<std::string>& files)
{
    char path[PATH_LEN] = {0};

    if (snprintf_s(path, sizeof(path), sizeof(path) - 1, "/proc/%d/task", pid) <= 0) {
        return false;
    }

    char realPath[PATH_MAX];
    if (!realpath(path, realPath)) {
        return false;
    }
    return ReadDirFiles(realPath, files);
}

bool GetTidsByPidWithFunc(const int pid, std::vector<int>& tids, std::function<bool(int)> const& func)
{
    std::vector<std::string> files;
    if (ReadDirFilesByPid(pid, files)) {
        for (size_t i = 0; i < files.size(); ++i) {
            pid_t tid = atoi(files[i].c_str());
            if (tid == 0) {
                continue;
            }
            tids.push_back(tid);

            if (func != nullptr) {
                func(tid);
            }
        }
    }
    return (tids.size() > 0);
}

bool GetTidMapByPid(const int pid, std::unordered_map<pid_t, pid_t>& tid_nstid_map)
{
    struct ProcInfo procInfo;
    (void)GetProcStatusByPid(pid, procInfo);

    std::function<bool(int)> func = nullptr;
    if (procInfo.ns) {
        func = [&](int tid) {
            pid_t nstid = tid;
            TidToNstid(pid, tid, nstid);
            tid_nstid_map[tid] = nstid;
            return true;
        };
    }
    std::vector<int> real_tids;
    (void)GetTidsByPidWithFunc(pid, real_tids, func);
    return (tid_nstid_map.size() > 0);
}
} // namespace crashpad
