//
// Copyright (C) 2012 Opensim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "ModulePathAddress.h"

bool ModulePathAddress::tryParse(const char *addr) {
    cModule * module = simulation.getSystemModule()->getModuleByRelativePath(addr);
    if (module) {
        id = module->getId();
        return true;
    }
    else
        return false;
}

std::string ModulePathAddress::str() const {
    cModule * module = simulation.getModule(id);
    std::string fullPath = module->getFullPath();
    return strchr(fullPath.c_str(), '.') + 1;
}
