/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#if defined(OHOS_CRASHPAD)
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <mutex>


#include "base/logging.h"
#include "base/command_line.h"
#include "crashpad_dfx.h"
#include "crashpad_dfx_elf_define.h"
#include "third_party/ohos_ndk/includes/ohos_adapter/ohos_adapter_helper.h"

// For process crash constants
constexpr char BUNDLE_NAME[] = "BUNDLE_NAME";
constexpr char PROCESS_CRASH[] = "PROCESS_CRASH";
constexpr char PROCESS_TYPE[] = "PROCESS_TYPE";
constexpr char CRASH_COUNT[] = "CRASH_COUNT";
constexpr char ERROR_REASON[] = "ERROR_REASON";

// For process crash log constants
constexpr char UID[] = "UID";
constexpr char BUILDID[] = "BUILDID";
constexpr char HAPPEN_TIME[] = "HAPPEN_TIME";
constexpr char USERID[] = "USERID";

extern uid_t g_process_uid;
extern std::string g_bundle_name;
extern std::string g_happen_time;

namespace crashpad {

std::mutex render_crash_count_mutex;
static int render_crash_count = 0;

int CrashpadDfx::GetAndUpdateRenderProcessCrashCount(const std::string process_type) {
    if (process_type == "browser") {
        return 1;
    }
    std::lock_guard<std::mutex> lock(render_crash_count_mutex);
    return ++render_crash_count;
}

//for get process_crash happen_time
std::string CrashpadDfx::GetCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d%H%M%S");
    return ss.str();
}

//for get process_crash uid
uid_t CrashpadDfx::GetEffectiveProcessUID() {
    if (g_process_uid == 0) {
        return getuid();
    }
    return g_process_uid;
}

// for get process_crash BUILDID
std::string CrashpadDfx::GetBuildId(const uint64_t noteAddr,const uint64_t noteSize) {
    uint64_t tmp;
    if (__builtin_add_overflow(noteAddr, noteSize, &tmp)) {
        LOG(ERROR) << "noteAddr overflow";
        return "";
    }
    uint64_t offset = 0;
    while (offset < noteSize) {
        ElfW(Nhdr) nhdr;
        if (noteSize - offset < sizeof(nhdr)) {
            LOG(ERROR) << "Insufficient space for reading nhdr at offset " << offset;
            return "";
        }
        memcpy(&nhdr, reinterpret_cast<void*>(noteAddr + offset), sizeof(nhdr));
        LOG(DEBUG) << "Note header read at offset " << offset << ": namesz=" << nhdr.n_namesz 
        << ", descsz=" << nhdr.n_descsz << ", type=" << nhdr.n_type;
        offset += sizeof(nhdr);

        if (noteSize - offset < nhdr.n_namesz) {
            LOG(ERROR) << "Insufficient space for reading note name at offset " << offset;
            return "";
        }

        std::string name(nhdr.n_namesz, '\0');
        memcpy(&(name[0]), reinterpret_cast<void*>(noteAddr + offset), nhdr.n_namesz);
        if (name.back() == '\0') {
            name.resize(name.size() - 1); // Trim trailing '\0'
        }
        offset += (nhdr.n_namesz + 3) & ~3; // Align to 4 bytes

        if (name == "GNU" && nhdr.n_type == NT_GNU_BUILD_ID) {
            if (noteSize - offset < nhdr.n_descsz || nhdr.n_descsz == 0) {
                LOG(ERROR) << "Insufficient space for reading build ID at offset " << offset;
                return "";
            }
            std::string buildIdRaw(nhdr.n_descsz, '\0');
            memcpy(&buildIdRaw[0], reinterpret_cast<void*>(noteAddr + offset), nhdr.n_descsz);

            std::string buildIdHex;
            for (unsigned char c : buildIdRaw) {
                char hex[3];
                snprintf(hex, sizeof(hex), "%02x", c);
                buildIdHex += hex;
            }
            LOG(INFO) << "Build ID extracted successfully: " << buildIdHex;
            return buildIdHex;
        }

        offset += (nhdr.n_descsz + 3) & ~3; // Align to 4 bytes
    }
    LOG(DEBUG) << "No GNU build ID found.";
    return "";
}

std::string CrashpadDfx::GetBuildIdFromSO(const std::string& soFilePath) {
    LOG(DEBUG) << "Attempting to open file: " << soFilePath;
    int fd = open(soFilePath.c_str(), O_RDONLY);
    if (fd < 0) {
        LOG(ERROR) << "Failed to open file: " << soFilePath << ". Error: " << strerror(errno);
        return "";
    }

    struct stat stat_buf;
    if (fstat(fd, &stat_buf) < 0) {
        LOG(ERROR) << "Failed to get file size: " << soFilePath << ". Error: " << strerror(errno);
        close(fd);
        return "";
    }

    void* mapped = mmap(nullptr, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        LOG(ERROR) << "Failed to map file: " << soFilePath << ". Error: " << strerror(errno);
        close(fd);
        return "";
    }

    Elf64_Ehdr* ehdr = reinterpret_cast<Elf64_Ehdr*>(mapped);
    Elf64_Shdr* shdrs = reinterpret_cast<Elf64_Shdr*>(reinterpret_cast<char*>(mapped) + ehdr->e_shoff);

    std::string buildId;
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        Elf64_Shdr& shdr = shdrs[i];
        if (shdr.sh_type == SHT_NOTE) {
            LOG(DEBUG) << "Found .note.gnu.build-id section.";
            buildId = GetBuildId(reinterpret_cast<uint64_t>(mapped) + shdr.sh_offset, shdr.sh_size);
            if (!buildId.empty()) {
                break;
            }
        }
    }

    if (buildId.empty()) {
        LOG(WARNING) << "No Build ID found in the file.";
    }

    munmap(mapped, stat_buf.st_size);
    close(fd);

    return buildId;
}

std::string CrashpadDfx::RetrieveBuildId() {
    static std::string cachedBuildID;
    if (!cachedBuildID.empty()) {
        return cachedBuildID;
    }
#if defined(__arm__)
    const std::string soFilePath = "/data/storage/el1/bundle/nweb/libs/arm/libweb_engine.so";
#elif defined(__aarch64__)
    const std::string soFilePath = "/data/storage/el1/bundle/nweb/libs/arm64/libweb_engine.so";
#elif defined(__x86_64__)
    const std::string soFilePath = "/data/storage/el1/bundle/nweb/libs/x86_64/libweb_engine.so";
#else
    cachedBuildID = "unsupported";
    return cachedBuildID;
#endif
    cachedBuildID = CrashpadDfx::GetBuildIdFromSO(soFilePath);
    return cachedBuildID;
}

std::string CrashpadDfx::GenerateFileName() {
    // 获取有效的进程 UID
    g_process_uid = GetEffectiveProcessUID();
    std::string buildID = RetrieveBuildId();
    return buildID;
}

std::string CrashpadDfx::GetProcessBundleName() {
    const base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Check if command_line is nullptr before using it
    if (!command_line) {
        LOG(ERROR) << "CommandLine object is null. Cannot retrieve bundle name.";
        return "";
    }
 
    if (command_line->HasSwitch("bundle-name")) {
        std::string bundle_name = command_line->GetSwitchValueASCII("bundle-name");
        if (bundle_name.empty()) {
            LOG(ERROR) << "Bundle Name command line parameter is provided but empty.";
            return "";
        }
        return bundle_name;
    } else {
        LOG(ERROR) << "Bundle Name not provided as a command line parameter.";
        return "";
    }
}

void CrashpadDfx::ProcessCrashReport(const std::string process_type,
                                     const std::string happen_time,
                                     const std::string bundle_name,
                                     const std::string error_reason) {
    const std::string build_id = RetrieveBuildId();
    uid_t uid = GetEffectiveProcessUID();
    uid_t user_id_t = getuid()/200000;
    const std::string user_id = std::to_string(user_id_t);;
    const std::string crash_count = std::to_string(GetAndUpdateRenderProcessCrashCount(process_type));
 
    OHOS::NWeb::OhosAdapterHelper::GetInstance().GetHiSysEventAdapterInstance().Write(
      PROCESS_CRASH,
      OHOS::NWeb::HiSysEventAdapter::EventType::FAULT,
    {
      BUNDLE_NAME, bundle_name,
      PROCESS_TYPE, process_type,
      ERROR_REASON, error_reason,
      CRASH_COUNT, crash_count,
      UID, std::to_string(uid),
      BUILDID, build_id,
      HAPPEN_TIME, happen_time,
      USERID, user_id
    });
}

std::string CrashpadDfx::UpdateCrashDumpPathSuffix() {
  const std::string crashpad = "crashpad";
  const std::string delimiter = "-";
  const std::string buildID = CrashpadDfx::GenerateFileName();
  const std::string bundle_name = g_bundle_name;
  const std::string happen_time = g_happen_time;
  const std::string user_id = std::to_string(getuid()/200000);
  const std::string dump_path_suffix = crashpad + delimiter + bundle_name  + delimiter 
  + std::to_string(g_process_uid) + delimiter + buildID + delimiter + happen_time;
  return dump_path_suffix;
}
#endif //defined(OHOS_CRASHPAD)
}  // namespace Crashpad