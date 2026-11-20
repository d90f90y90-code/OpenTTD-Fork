/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file android_main.cpp Main entry for Android; called by SDLActivity from Java. */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../core/random_func.hpp"
#include "../../string_func.h"

#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <unwind.h>
#include <stdio.h>
#include <stdlib.h>


static char _android_crash_path[512]; ///< File to write a native crash report to.

/** State of a backtrace that is being written to a file. */
struct BacktraceState {
	int fd; ///< File descriptor to write to.
	int depth; ///< Number of frames written.
};

static _Unwind_Reason_Code AndroidBacktraceCallback(struct _Unwind_Context *context, void *arg)
{
	auto *state = static_cast<BacktraceState *>(arg);
	uintptr_t pc = _Unwind_GetIP(context);
	if (pc == 0) return _URC_END_OF_STACK;

	char line[512];
	Dl_info info;
	int len;
	if (dladdr(reinterpret_cast<void *>(pc), &info) != 0 && info.dli_fname != nullptr) {
		long offset = info.dli_saddr != nullptr ? static_cast<long>(pc - reinterpret_cast<uintptr_t>(info.dli_saddr)) : 0;
		len = snprintf(line, sizeof(line), "#%02d pc %p %s (%s+%ld)\n", state->depth, reinterpret_cast<void *>(pc), info.dli_fname, info.dli_sname != nullptr ? info.dli_sname : "?", offset);
	} else {
		len = snprintf(line, sizeof(line), "#%02d pc %p\n", state->depth, reinterpret_cast<void *>(pc));
	}
	if (len > 0 && write(state->fd, line, len) < 0) return _URC_END_OF_STACK;

	return ++state->depth >= 48 ? _URC_END_OF_STACK : _URC_NO_REASON;
}

static void AndroidCrashHandler(int sig, siginfo_t *info, void *)
{
	int fd = open(_android_crash_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		char header[160];
		int len = snprintf(header, sizeof(header), "Native crash: signal %d, fault address %p\n", sig, info != nullptr ? info->si_addr : nullptr);
		if (len > 0 && write(fd, header, len) < 0) { /* Nothing we can do. */ }
		BacktraceState state{fd, 0};
		_Unwind_Backtrace(AndroidBacktraceCallback, &state);
		close(fd);
	}
	signal(sig, SIG_DFL);
	raise(sig);
}

/** Write native crashes and the game's error output to files in the app's private storage, so they can be shown on the next start. */
static void SetupAndroidDiagnostics()
{
	const char *home = getenv("HOME");
	if (home == nullptr) return;

	snprintf(_android_crash_path, sizeof(_android_crash_path), "%s/crash.txt", home);

	char log_path[512];
	snprintf(log_path, sizeof(log_path), "%s/openttd-stderr.txt", home);
	if (freopen(log_path, "w", stderr) != nullptr) setvbuf(stderr, nullptr, _IONBF, 0);

	struct sigaction sa{};
	sa.sa_sigaction = AndroidCrashHandler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) sigaction(sig, &sa, nullptr);
}

/**
 * Entry point looked up by SDL's Android activity in libmain.so.
 * The Java side sets HOME and the XDG_* variables to the app's private storage before this runs.
 */
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char *argv[])
{
	SetupAndroidDiagnostics();

	std::vector<std::string_view> params;
	for (int i = 0; i < argc; ++i) {
		StrMakeValidInPlace(argv[i]);
		params.emplace_back(argv[i]);
	}
	params.emplace_back("-d");
	params.emplace_back("driver=2,misc=1");

	SetRandomSeed(time(nullptr));

	signal(SIGPIPE, SIG_IGN);

	return openttd_main(params);
}
