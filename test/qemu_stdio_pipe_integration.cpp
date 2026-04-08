#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#ifndef OOS_QEMU_STDIO_IT_ROOT_DIR
#define OOS_QEMU_STDIO_IT_ROOT_DIR "."
#endif

#ifndef OOS_QEMU_STDIO_IT_BUILD_DIR
#define OOS_QEMU_STDIO_IT_BUILD_DIR "."
#endif

namespace
{
	struct Config
	{
		int promptTimeoutSec;
		int cmdTimeoutSec;
		int shutdownTimeoutSec;
		std::string qemuBin;
		std::string readyMarker;
		std::string token;
		std::regex dateRegex;
	};

	struct ChildProcess
	{
		pid_t pid;
		int stdinFd;
		int stdoutFd;
		int stderrFd;
		bool exited;
		int waitStatus;
	};

	int ReadEnvInt(const char* name, int fallback)
	{
		const char* raw = std::getenv(name);
		if ( raw == nullptr || *raw == '\0' )
		{
			return fallback;
		}

		char* end = nullptr;
		long value = std::strtol(raw, &end, 10);
		if ( end == raw || *end != '\0' || value <= 0 || value > 3600 )
		{
			std::cerr << "warning: invalid " << name << "='" << raw
				      << "', fallback to " << fallback << "\n";
			return fallback;
		}

		return static_cast<int>(value);
	}

	std::string ReadEnvString(const char* name, const std::string& fallback)
	{
		const char* raw = std::getenv(name);
		if ( raw == nullptr || *raw == '\0' )
		{
			return fallback;
		}
		return std::string(raw);
	}

	bool SetFdNonBlocking(int fd)
	{
		int flags = fcntl(fd, F_GETFL, 0);
		if ( flags < 0 )
		{
			std::perror("fcntl(F_GETFL)");
			return false;
		}

		if ( fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 )
		{
			std::perror("fcntl(F_SETFL)");
			return false;
		}

		return true;
	}

	void CloseFd(int& fd)
	{
		if ( fd >= 0 )
		{
			close(fd);
			fd = -1;
		}
	}

	bool UpdateChildExitStatus(ChildProcess& child)
	{
		if ( child.exited || child.pid <= 0 )
		{
			return true;
		}

		int status = 0;
		pid_t waited = waitpid(child.pid, &status, WNOHANG);
		if ( waited == 0 )
		{
			return true;
		}
		if ( waited == child.pid )
		{
			child.exited = true;
			child.waitStatus = status;
			return true;
		}
		if ( waited < 0 && errno != EINTR )
		{
			std::perror("waitpid(WNOHANG)");
			return false;
		}

		return true;
	}

	bool DrainFd(int& fd, std::string& buffer, std::ofstream& logStream)
	{
		if ( fd < 0 )
		{
			return true;
		}

		char chunk[4096];
		while ( true )
		{
			ssize_t n = read(fd, chunk, sizeof(chunk));
			if ( n > 0 )
			{
				buffer.append(chunk, static_cast<size_t>(n));
				logStream.write(chunk, n);
				logStream.flush();
				continue;
			}
			if ( n == 0 )
			{
				CloseFd(fd);
				return true;
			}
			if ( errno == EINTR )
			{
				continue;
			}
			if ( errno == EAGAIN || errno == EWOULDBLOCK )
			{
				return true;
			}

			std::perror("read");
			return false;
		}
	}

	bool PumpChildIo(
		ChildProcess& child,
		std::string& stdoutBuffer,
		std::string& stderrBuffer,
		std::ofstream& stdoutLog,
		std::ofstream& stderrLog,
		int timeoutMs)
	{
		struct Endpoint
		{
			int* fd;
			std::string* buffer;
			std::ofstream* log;
		};

		std::vector<pollfd> pollfds;
		std::vector<Endpoint> endpoints;

		if ( child.stdoutFd >= 0 )
		{
			pollfds.push_back({ child.stdoutFd, POLLIN | POLLHUP | POLLERR, 0 });
			endpoints.push_back({ &child.stdoutFd, &stdoutBuffer, &stdoutLog });
		}
		if ( child.stderrFd >= 0 )
		{
			pollfds.push_back({ child.stderrFd, POLLIN | POLLHUP | POLLERR, 0 });
			endpoints.push_back({ &child.stderrFd, &stderrBuffer, &stderrLog });
		}

		if ( !pollfds.empty() )
		{
			int pollRet = poll(pollfds.data(), pollfds.size(), timeoutMs);
			if ( pollRet < 0 && errno != EINTR )
			{
				std::perror("poll");
				return false;
			}

			if ( pollRet > 0 )
			{
				for ( size_t i = 0; i < pollfds.size(); ++i )
				{
					if ( (pollfds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0 )
					{
						continue;
					}

					if ( !DrainFd(*endpoints[i].fd, *endpoints[i].buffer, *endpoints[i].log) )
					{
						return false;
					}
				}
			}
		}
		else
		{
			usleep(static_cast<useconds_t>(timeoutMs * 1000));
		}

		return UpdateChildExitStatus(child);
	}

	bool WriteAll(int fd, std::string_view data)
	{
		size_t offset = 0;
		while ( offset < data.size() )
		{
			ssize_t n = write(fd, data.data() + offset, data.size() - offset);
			if ( n > 0 )
			{
				offset += static_cast<size_t>(n);
				continue;
			}
			if ( n < 0 && errno == EINTR )
			{
				continue;
			}

			std::perror("write");
			return false;
		}

		return true;
	}

	void PrintTail(const std::string& title, const std::string& content)
	{
		const size_t maxChars = 4000;
		std::cerr << "--- " << title << " tail ---\n";
		if ( content.size() <= maxChars )
		{
			std::cerr << content << "\n";
			return;
		}
		std::cerr << content.substr(content.size() - maxChars) << "\n";
	}

	void TerminateChild(ChildProcess& child)
	{
		if ( child.pid <= 0 || child.exited )
		{
			return;
		}

		kill(child.pid, SIGTERM);
		for ( int i = 0; i < 60; ++i )
		{
			if ( UpdateChildExitStatus(child) && child.exited )
			{
				return;
			}
			usleep(50000);
		}

		kill(child.pid, SIGKILL);
		int status = 0;
		if ( waitpid(child.pid, &status, 0) == child.pid )
		{
			child.exited = true;
			child.waitStatus = status;
		}
	}

	int ExitCodeFromWaitStatus(int waitStatus)
	{
		if ( WIFEXITED(waitStatus) )
		{
			return WEXITSTATUS(waitStatus);
		}
		if ( WIFSIGNALED(waitStatus) )
		{
			return 128 + WTERMSIG(waitStatus);
		}
		return 255;
	}

	ChildProcess SpawnQemuStdio(
		const std::filesystem::path& rootDir,
		const std::filesystem::path& buildDir,
		const Config& config)
	{
		int stdinPipe[2] = { -1, -1 };
		int stdoutPipe[2] = { -1, -1 };
		int stderrPipe[2] = { -1, -1 };

		if ( pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0 || pipe(stderrPipe) < 0 )
		{
			std::perror("pipe");
			CloseFd(stdinPipe[0]);
			CloseFd(stdinPipe[1]);
			CloseFd(stdoutPipe[0]);
			CloseFd(stdoutPipe[1]);
			CloseFd(stderrPipe[0]);
			CloseFd(stderrPipe[1]);
			return { -1, -1, -1, -1, false, 0 };
		}

		pid_t pid = fork();
		if ( pid < 0 )
		{
			std::perror("fork");
			CloseFd(stdinPipe[0]);
			CloseFd(stdinPipe[1]);
			CloseFd(stdoutPipe[0]);
			CloseFd(stdoutPipe[1]);
			CloseFd(stderrPipe[0]);
			CloseFd(stderrPipe[1]);
			return { -1, -1, -1, -1, false, 0 };
		}

		if ( pid == 0 )
		{
			if ( dup2(stdinPipe[0], STDIN_FILENO) < 0 ||
			     dup2(stdoutPipe[1], STDOUT_FILENO) < 0 ||
			     dup2(stderrPipe[1], STDERR_FILENO) < 0 )
			{
				std::perror("dup2");
				_exit(127);
			}

			CloseFd(stdinPipe[0]);
			CloseFd(stdinPipe[1]);
			CloseFd(stdoutPipe[0]);
			CloseFd(stdoutPipe[1]);
			CloseFd(stderrPipe[0]);
			CloseFd(stderrPipe[1]);

			if ( chdir(rootDir.c_str()) < 0 )
			{
				std::perror("chdir");
				_exit(127);
			}

			setenv("QEMU_MODE", "stdio", 1);
			setenv("QEMU_BIN", config.qemuBin.c_str(), 1);
			setenv("OOS_BUILD_DIR", buildDir.c_str(), 1);

			std::string scriptPath = (rootDir / "qemu" / "run_qemu.sh").string();
			execl("/bin/bash", "bash", scriptPath.c_str(), static_cast<char*>(nullptr));
			std::perror("execl");
			_exit(127);
		}

		CloseFd(stdinPipe[0]);
		CloseFd(stdoutPipe[1]);
		CloseFd(stderrPipe[1]);

		return { pid, stdinPipe[1], stdoutPipe[0], stderrPipe[0], false, 0 };
	}

	bool WaitForCondition(
		const std::string& description,
		int timeoutSec,
		ChildProcess& child,
		std::string& stdoutBuffer,
		std::string& stderrBuffer,
		std::ofstream& stdoutLog,
		std::ofstream& stderrLog,
		const std::function<bool()>& condition)
	{
		using Clock = std::chrono::steady_clock;
		auto deadline = Clock::now() + std::chrono::seconds(timeoutSec);

		while ( Clock::now() < deadline )
		{
			if ( condition() )
			{
				return true;
			}

			if ( !PumpChildIo(child, stdoutBuffer, stderrBuffer, stdoutLog, stderrLog, 200) )
			{
				return false;
			}

			if ( condition() )
			{
				return true;
			}

			if ( child.exited )
			{
				std::cerr << "qemu exited while waiting for " << description << "\n";
				return false;
			}
		}

		std::cerr << "timeout after " << timeoutSec << "s waiting for " << description << "\n";
		return false;
	}

	bool SendCommand(int fd, const std::string& cmd)
	{
		std::string line = cmd;
		line.push_back('\n');
		return WriteAll(fd, line);
	}

	bool ContainsFrom(const std::string& text, size_t start, const std::string& needle)
	{
		if ( start > text.size() )
		{
			return false;
		}
		return text.find(needle, start) != std::string::npos;
	}

	bool RegexSearchFrom(const std::string& text, size_t start, const std::regex& pattern)
	{
		if ( start >= text.size() )
		{
			return false;
		}

		std::string chunk = text.substr(start);
		return std::regex_search(chunk, pattern);
	}

	bool HasShellPromptFrom(const std::string& text, size_t start)
	{
		if ( start >= text.size() )
		{
			return false;
		}

		size_t promptPos = text.rfind("]#");
		if ( promptPos == std::string::npos || promptPos < start )
		{
			return false;
		}

		size_t bracketPos = text.rfind('[', promptPos);
		return bracketPos != std::string::npos && bracketPos >= start;
	}

	bool HasNextShellPromptFrom(const std::string& text, size_t start)
	{
		if ( start >= text.size() )
		{
			return false;
		}

		size_t pos = text.find("\n[", start);
		while ( pos != std::string::npos )
		{
			size_t end = text.find("]#", pos + 2);
			if ( end != std::string::npos )
			{
				return true;
			}
			pos = text.find("\n[", pos + 2);
		}

		return false;
	}

	bool HasShellPrompt(const std::string& text)
	{
		size_t promptPos = text.rfind("]#");
		if ( promptPos == std::string::npos )
		{
			return false;
		}

		size_t bracketPos = text.rfind('[', promptPos);
		return bracketPos != std::string::npos;
	}
}

int main()
{
	const Config config = {
		ReadEnvInt("QEMU_STDIO_PROMPT_TIMEOUT_SEC", 90),
		ReadEnvInt("QEMU_STDIO_CMD_TIMEOUT_SEC", 30),
		ReadEnvInt("QEMU_STDIO_SHUTDOWN_TIMEOUT_SEC", 45),
		ReadEnvString("QEMU_BIN", "qemu-system-i386"),
		ReadEnvString("QEMU_STDIO_READY_MARKER", "OOS_BOOT_SHELL_READY"),
		"__QEMU_STDIO_PIPE_OK__",
		/*
		 * 内核 date 命令当前会输出形如 "(NOT Used)" 的占位后缀，
		 * 这里仅校验日期时间主体与括号结构，不再把括号内容写死为 3 字母时区。
		 */
		std::regex("[0-9]{1,2}-[A-Za-z]{3}-[0-9]{4} [0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2}\\([^\\)]*\\)")
	};

	std::filesystem::path rootDir = ReadEnvString("OOS_ROOT_DIR", OOS_QEMU_STDIO_IT_ROOT_DIR);
	std::filesystem::path runScript = rootDir / "qemu" / "run_qemu.sh";
	std::filesystem::path buildDir = ReadEnvString("OOS_BUILD_DIR", OOS_QEMU_STDIO_IT_BUILD_DIR);
	std::filesystem::path imagePath = buildDir / "c.img";
	std::filesystem::path stdoutLogPath = buildDir / "qemu-stdio-integration-stdout.log";
	std::filesystem::path stderrLogPath = buildDir / "qemu-stdio-integration-stderr.log";

	if ( !std::filesystem::exists(runScript) )
	{
		std::cerr << "missing qemu runner: " << runScript << "\n";
		return 1;
	}
	if ( !std::filesystem::exists(imagePath) )
	{
		std::cerr << "missing image: " << imagePath << "\n";
		return 1;
	}
	if ( !std::filesystem::create_directories(buildDir) && !std::filesystem::exists(buildDir) )
	{
		std::cerr << "failed to create build dir: " << buildDir << "\n";
		return 1;
	}

	std::ofstream stdoutLog(stdoutLogPath, std::ios::out | std::ios::trunc | std::ios::binary);
	std::ofstream stderrLog(stderrLogPath, std::ios::out | std::ios::trunc | std::ios::binary);
	if ( !stdoutLog.is_open() || !stderrLog.is_open() )
	{
		std::cerr << "failed to open log files in " << buildDir << "\n";
		return 1;
	}

	std::cout << "qemu stdio integration (C++): prompt_timeout=" << config.promptTimeoutSec
		      << "s cmd_timeout=" << config.cmdTimeoutSec
		      << "s shutdown_timeout=" << config.shutdownTimeoutSec << "s\n";

	ChildProcess child = SpawnQemuStdio(rootDir, buildDir, config);
	if ( child.pid <= 0 )
	{
		return 1;
	}

	if ( !SetFdNonBlocking(child.stdoutFd) || !SetFdNonBlocking(child.stderrFd) )
	{
		TerminateChild(child);
		CloseFd(child.stdinFd);
		CloseFd(child.stdoutFd);
		CloseFd(child.stderrFd);
		return 1;
	}

	std::cout << "qemu stdio integration: qemu pid=" << child.pid << "\n";

	std::string stdoutBuffer;
	std::string stderrBuffer;
	bool ok = true;
	const std::string copyTarget = "/var/gcopy";
	const std::string rebootToken = config.token + "_REBOOT";

	ok = ok && WaitForCondition(
		"shell prompt",
		config.promptTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return HasShellPrompt(stdoutBuffer); });

	ok = ok && WaitForCondition(
		"ready marker in stderr",
		config.promptTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return ContainsFrom(stderrBuffer, 0, config.readyMarker); });

	size_t stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "echo " + config.token) )
		{
			std::cerr << "failed to send echo command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"echo token output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return ContainsFrom(stdoutBuffer, stdoutCursor, config.token); });

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "date") )
		{
			std::cerr << "failed to send date command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"date output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return RegexSearchFrom(stdoutBuffer, stdoutCursor, config.dateRegex); });

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "HelloWorld") )
		{
			std::cerr << "failed to send HelloWorld command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"HelloWorld output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return ContainsFrom(stdoutBuffer, stdoutCursor, "Welcome to Unix V6++!"); });

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "argvdump foo bar") )
		{
			std::cerr << "failed to send argvdump command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"argvdump output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() {
			return ContainsFrom(stdoutBuffer, stdoutCursor, "argc=3") &&
				ContainsFrom(stdoutBuffer, stdoutCursor, "argv[1]='foo'") &&
				ContainsFrom(stdoutBuffer, stdoutCursor, "argv[2]='bar'");
		});

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "cat /etc/greetings!") )
		{
			std::cerr << "failed to send cat /etc/greetings! command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"cat /etc/greetings! output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() {
			return ContainsFrom(stdoutBuffer, stdoutCursor, "greetings from tongji") &&
				HasShellPromptFrom(stdoutBuffer, stdoutCursor);
		});

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "cp /etc/greetings! " + copyTarget) )
		{
			std::cerr << "failed to send cp command\n";
			ok = false;
		}
	}

	bool cpNeedsConfirm = false;
	ok = ok && WaitForCondition(
		"cp command completion or overwrite prompt",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() {
			if ( ContainsFrom(stdoutBuffer, stdoutCursor, "Do you want to cover the file") )
			{
				cpNeedsConfirm = true;
				return true;
			}
			return HasNextShellPromptFrom(stdoutBuffer, stdoutCursor);
		});

	if ( ok && cpNeedsConfirm )
	{
		size_t confirmCursor = stdoutBuffer.size();
		if ( !SendCommand(child.stdinFd, "y") )
		{
			std::cerr << "failed to confirm cp overwrite prompt\n";
			ok = false;
		}

		ok = ok && WaitForCondition(
			"cp overwrite confirm completion",
			config.cmdTimeoutSec,
			child,
			stdoutBuffer,
			stderrBuffer,
			stdoutLog,
			stderrLog,
			[&]() { return HasNextShellPromptFrom(stdoutBuffer, confirmCursor); });
	}

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "cat " + copyTarget) )
		{
			std::cerr << "failed to send cat copied file command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"cat copied file output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() {
			return ContainsFrom(stdoutBuffer, stdoutCursor, "greetings from tongji") &&
				HasShellPromptFrom(stdoutBuffer, stdoutCursor);
		});

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "ls /bin") )
		{
			std::cerr << "failed to send ls /bin command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"ls /bin output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() {
			return ContainsFrom(stdoutBuffer, stdoutCursor, "Directory '/bin':") &&
				ContainsFrom(stdoutBuffer, stdoutCursor, "echo") &&
				ContainsFrom(stdoutBuffer, stdoutCursor, "date") &&
				ContainsFrom(stdoutBuffer, stdoutCursor, "reboot") &&
				(ContainsFrom(stdoutBuffer, stdoutCursor, "utest") ||
					ContainsFrom(stdoutBuffer, stdoutCursor, "test"));
		});

	stdoutCursor = stdoutBuffer.size();
	size_t stderrCursor = stderrBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "reboot") )
		{
			std::cerr << "failed to send reboot command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"reboot request output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return ContainsFrom(stdoutBuffer, stdoutCursor, "Requesting reboot"); });

	ok = ok && WaitForCondition(
		"shell prompt after reboot",
		config.promptTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return HasShellPromptFrom(stdoutBuffer, stdoutCursor); });

	ok = ok && WaitForCondition(
		"ready marker after reboot",
		config.promptTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return ContainsFrom(stderrBuffer, stderrCursor, config.readyMarker); });

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "") )
		{
			std::cerr << "failed to send sync newline after reboot\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"sync prompt after reboot",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return HasNextShellPromptFrom(stdoutBuffer, stdoutCursor); });

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "echo " + rebootToken) )
		{
			std::cerr << "failed to send echo after reboot\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"echo token after reboot",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() {
			return ContainsFrom(stdoutBuffer, stdoutCursor, rebootToken) &&
				HasShellPromptFrom(stdoutBuffer, stdoutCursor);
		});

	stdoutCursor = stdoutBuffer.size();
	if ( ok )
	{
		if ( !SendCommand(child.stdinFd, "shutdown") )
		{
			std::cerr << "failed to send shutdown command\n";
			ok = false;
		}
	}

	ok = ok && WaitForCondition(
		"shutdown request output",
		config.cmdTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return ContainsFrom(stdoutBuffer, stdoutCursor, "Requesting power off"); });

	ok = ok && WaitForCondition(
		"qemu process exit",
		config.shutdownTimeoutSec,
		child,
		stdoutBuffer,
		stderrBuffer,
		stdoutLog,
		stderrLog,
		[&]() { return child.exited; });

	if ( !ok )
	{
		TerminateChild(child);
	}

	PumpChildIo(child, stdoutBuffer, stderrBuffer, stdoutLog, stderrLog, 0);

	CloseFd(child.stdinFd);
	CloseFd(child.stdoutFd);
	CloseFd(child.stderrFd);

	if ( !ok )
	{
		PrintTail("qemu stdio integration stdout", stdoutBuffer);
		PrintTail("qemu stdio integration stderr", stderrBuffer);
		return 1;
	}

	const int qemuExitCode = ExitCodeFromWaitStatus(child.waitStatus);
	if ( WIFSIGNALED(child.waitStatus) )
	{
		std::cerr << "qemu terminated by signal " << WTERMSIG(child.waitStatus) << "\n";
		PrintTail("qemu stdio integration stdout", stdoutBuffer);
		PrintTail("qemu stdio integration stderr", stderrBuffer);
		return 1;
	}

	std::cout << "qemu stdio integration passed: qemu exit code=" << qemuExitCode << "\n";
	return 0;
}
