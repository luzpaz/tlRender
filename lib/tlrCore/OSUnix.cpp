// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021 Darby Johnston
// All rights reserved.

#include <tlrCore/OS.h>

#if defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreServices/CoreServices.h>
#endif // __APPLE__

#include <sstream>

#include <sys/ioctl.h>
#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#else // __APPLE__
#include <sys/sysinfo.h>
#endif // __APPLE__
#include <sys/utsname.h>
#include <pwd.h>
#include <unistd.h>

namespace tlr
{
    namespace os
    {
        char getEnvListSeparator()
        {
            return getEnvListSeparator(EnvListSeparator::Unix);
        }
		
		std::string getInfo()
		{
			std::string out;
			::utsname info;
			uname(&info);
			std::stringstream s;
			s << info.sysname << " " << info.release << " " << info.machine;
			out = s.str();
			return out;
		}
		
		size_t getRAMSize()
		{
			size_t out = 0;
#if defined(__APPLE__)
			int name[2] = { CTL_HW, HW_MEMSIZE };
			u_int namelen = sizeof(name) / sizeof(name[0]);
			uint64_t size = 0;
			size_t len = sizeof(size);
			if (0 == sysctl(name, namelen, &size, &len, NULL, 0))
			{
				out = static_cast<size_t>(size);
			}
#else // __APPLE__
			struct sysinfo info;
			if (0 == sysinfo(&info))
			{
				out = info.totalram;
			}
#endif // __APPLE__
			return out;
		}

		bool getEnv(const std::string& name, std::string& out)
		{
			if (const char* p = ::getenv(name.c_str()))
			{
				out = std::string(p);
				return true;
			}
			return false;
		}

		bool setEnv(const std::string& name, const std::string& value)
		{
			return ::setenv(name.c_str(), value.c_str(), 1) == 0;
		}
	
		bool delEnv(const std::string& name)
		{
			return ::unsetenv(name.c_str()) == 0;
		}
    }
}