/*******************************************************************************
 *
 * Copyright (c) 2016 Seller Tamás. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *******************************************************************************/

#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTest/MemoryLeakWarningPlugin.h"

#include "Oops.h"
#include "FailureInjectorPlugin.h"
#include "Addr2lineBacktrace.h"

void oops() {
	FailureInjectorPlugin::instance()->printLastTrace();
}

void always() {
	std::cout << "\033[0m" << std::endl;
}

int main(int ac, char** av)
{
	OopsUtils<oops, always>::registerOopsHandlers();

	FailureInjectorPlugin::instance()->setBacktraceFactory(new Addr2lineBacktraceFactory);
	TestRegistry::getCurrentRegistry()->installPlugin(FailureInjectorPlugin::instance());
    MemoryLeakWarningPlugin::destroyGlobalDetector();

    auto ret = CommandLineTestRunner::RunAllTests(ac, av);

    OopsUtils<oops, always>::allWentOk();

    return ret;
}
