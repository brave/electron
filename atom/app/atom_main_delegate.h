// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_APP_ATOM_MAIN_DELEGATE_H_
#define ATOM_APP_ATOM_MAIN_DELEGATE_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "brightray/common/content_client.h"
#include "brightray/common/main_delegate.h"

namespace base {
class CommandLine;
}

namespace atom {

base::FilePath GetResourcesPakFilePathByName(const std::string resource_name);

class AtomMainDelegate : public brightray::MainDelegate {
 public:
  static const char* const kNonWildcardDomainNonPortSchemes[];
  static const size_t kNonWildcardDomainNonPortSchemesSize;

  AtomMainDelegate();

  // |exe_entry_point_ticks| is the time at which the main function of the
  // executable was entered, or null if not available.
  explicit AtomMainDelegate(base::TimeTicks exe_entry_point_ticks);

  ~AtomMainDelegate() override;

 protected:
  // content::ContentMainDelegate:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void SandboxInitialized(const std::string& process_type) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void ZygoteForked() override;
#endif
  void PreContentInitialization() override;

#if defined(OS_MACOSX)
  bool ShouldSendMachPort(const std::string& process_type) override;
  bool DelaySandboxInitialization(const std::string& process_type) override;
#endif

  // brightray::MainDelegate:
  std::unique_ptr<brightray::ContentClient> CreateContentClient() override;
#if defined(OS_MACOSX)
  void OverrideChildProcessPath() override;
  void OverrideFrameworkBundlePath() override;
#endif
  void ProcessExiting(const std::string& process_type) override;

 private:
#if defined(OS_MACOSX)
  void SetUpBundleOverrides();
  void InitMacCrashReporter(
      base::CommandLine* command_line,
      const std::string& process_type);
#endif

  brightray::ContentClient content_client_;
  std::unique_ptr<content::ContentBrowserClient> browser_client_;
  std::unique_ptr<content::ContentRendererClient> renderer_client_;
  std::unique_ptr<content::ContentUtilityClient> utility_client_;

  DISALLOW_COPY_AND_ASSIGN(AtomMainDelegate);
};

}  // namespace atom

#endif  // ATOM_APP_ATOM_MAIN_DELEGATE_H_
