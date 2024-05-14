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
#include <stdint.h>
#include <time.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "util/file/file_io.h"
#include "util/file/file_reader.h"
#include "util/file/file_writer.h"
#include "util/file/scoped_remove_file.h"
#include "util/misc/metrics.h"
#include "util/misc/uuid.h"

namespace crashpad {

class CrashpadDfx {

public:

    static void ReportProcessCrash(const std::string process_type,
                                    const std::string happen_time,
                                    const std::string package_name);
    static std::string GetProcessBundleName();
    static std::string GetCurrentTime();
    static std::string UpdateCrashDumpPathSuffix();
private:
    static std::string RetrieveBuildId();
    static std::string GetBuildId(const uint64_t noteAddr,const uint64_t noteSize);
    static std::string GetBuildIdFromSO(const std::string& soFilePath);
    static uid_t GetEffectiveProcessUID();
    static int GetAndUpdateRenderProcessCrashCount(const std::string process_type);
    static std::string GenerateFileName();
};

}// namespace crashpad_dfx
#endif // defined(OHOS_CRASHPAD)